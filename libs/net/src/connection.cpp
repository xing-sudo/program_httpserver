#include "connection.h"

#include <cassert>
#include <functional>

#include "logger.h"

void Connection::HandleRead()
{
    // 1.接收socket数据，放到输入缓冲区
    char buff[65535];
    ssize_t ret = _socket.NonBlockRecv(buff, sizeof(buff));
    if (ret < 0)
    {
        // 出错不能直接关闭连接，进一步判断buff状态
        return ShutdownInLoop();
    }
    // 将数据放到我们的输入缓冲区
    _inbuffer.Write(buff, ret);
    // 2.调用消息回调函数，处理输入缓冲区数据
    if (_inbuffer.ReadAbleSize() > 0)
    {
        // shared_from_this()获取当前对象的shared_ptr智能指针
        _message_callback(shared_from_this(), &_inbuffer);
    }
}

void Connection::HandleWrite()
{
    // outbuffer中就是要发送的数据
    ssize_t ret = _socket.NonBlockSend(_outbuffer.GetReadPos(), _outbuffer.ReadAbleSize());
    if (ret < 0)
    {
        // 发送错误关闭连接，但要判断buffer中是否还有数据
        if (_inbuffer.ReadAbleSize() > 0)
        {
            // 如果输入缓冲区还有数据，说明连接还活跃，不应该直接关闭连接，而是调用消息回调函数处理输入缓冲区中的数据
            _message_callback(shared_from_this(), &_inbuffer);
        }
        return Release();
    }
    _outbuffer.MoveReadPos(ret);
    if (_outbuffer.ReadAbleSize() == 0)
    {
        _channel.DisableWrite(); // 没有数据待发送了关闭写监控
        if (_status == DISCONNECTING)
        {
            // 如果连接状态是待关闭,说明之前是因为还有数据没有处理导致没有关闭连接
            return Release();
        }
    }
    return;
}

void Connection::HandleClose()
{
    // 连接关闭，套接字什么都不干了判断后直接关闭
    if (_inbuffer.ReadAbleSize() > 0)
    {
        _message_callback(shared_from_this(), &_inbuffer);
    }
    return Release();
}

void Connection::HandleAnyEvent()
{
    // 触发任何事件都刷新活跃度，调用使用者设置的任意事件回调
    if (_enable_inactive_release == true)
    {
        _loop->TimerRefresh(_connection_id);
    }
    if (_any_event_callback)
    {
        _any_event_callback(shared_from_this());
    }
}

void Connection::HandleError()
{
    return HandleClose(); // 直接复用close
}

void Connection::EstablishedInLoop()
{
    // 1.改状态
    assert(_status == CONNECTING);
    _status = CONNECTED;
    // 2.启动读监控
    _channel.EnableRead();
    _channel.Tie(shared_from_this()); // 绑定监控对象，防止连接对象被销毁后channel还在访问它
    // 3.调用用户设置的回调
    if (_connected_callback)
    {
        _connected_callback(shared_from_this());
    }
}

void Connection::ReleaseInLoop()
{
    // 1.修改连接状态
    _status = DISCONNECTED;
    // 2.移除事件监控
    _channel.Remove();
    // 3.关闭描述符
    _socket.Close();
    // 4.如果当前连接在定时器队列中还有定时销毁任务则取消
    if (_loop->HasTimer(_connection_id))
    {
        CancelInactiveReleaseInLoop();
    }
    // 5.调用关闭回调，避免先调用服务器回调导致连接释放，后续操作非法
    if (_istie_callback)
    {
        _istie_callback(shared_from_this());
    }
    // 移除服务器内部管理信息
    if (_server_istie_callback)
    {
        _server_istie_callback(shared_from_this());
    }
}

void Connection::SendInLoop(Buffer &buf)
{
    if (_status == DISCONNECTED)
    {
        return;
    }
    _outbuffer.Write(buf);
    if (_channel.WriteAble() == false)
    {
        _channel.EnableWrite();
    }
}

void Connection::ShutdownInLoop()
{
    // 设置待关闭
    _status = DISCONNECTING;
    // 如果输入缓冲区有数据就读取
    if (_inbuffer.ReadAbleSize() > 0)
    {
        if (_message_callback)
        {
            _message_callback(shared_from_this(), &_inbuffer);
        }
    }
    // 如果输出缓冲区有数据就发送
    if (_outbuffer.ReadAbleSize() > 0)
    {
        if (_channel.WriteAble() == false)
        {
            _channel.EnableWrite();
        }
    }
    // 所有都处理完了就关闭
    if (_outbuffer.ReadAbleSize() == 0)
    {
        Release();
    }
}

void Connection::EnableInactiveReleaseInLoop(int sec)
{
    // 更改标志位
    _enable_inactive_release = true;
    // 判断是否有定时销毁任务有就刷新
    if (_loop->HasTimer(_connection_id))
    {
        return _loop->TimerRefresh(_connection_id);
    }
    // 没有则添加
    _loop->TimerAdd(_connection_id, sec, std::bind(&Connection::Release, this));
}

void Connection::CancelInactiveReleaseInLoop()
{
    // 更改标志位
    _enable_inactive_release = false;
    // 有定时销毁就取消
    if (_loop->HasTimer(_connection_id))
    {
        _loop->TimerCancel(_connection_id);
    }
}

void Connection::UpgradeInLoop(const Any &context, const ConnectedCallBack &con,
                               const MessageCallBack &msg, const ClosedCallBack &closed,
                               const AnyEventCallBack &any)
{
    _context = context;
    _connected_callback = con;
    _message_callback = msg;
    _istie_callback = closed;
    _any_event_callback = any;
}

Connection::Connection(EventLoop *loop, uint64_t id, int sockfd) : _connection_id(id), _sockfd(sockfd), _loop(loop),
                                                                   _enable_inactive_release(false), _status(CONNECTING), _socket(_sockfd), _channel(loop, sockfd)
{
    _channel.SetReadCallBack(std::bind(&Connection::HandleRead, this));
    _channel.SetWriteCallBack(std::bind(&Connection::HandleWrite, this));
    _channel.SetCloseCallBack(std::bind(&Connection::HandleClose, this));
    _channel.SetEventCallBack(std::bind(&Connection::HandleAnyEvent, this));
    _channel.SetErrorCallBack(std::bind(&Connection::HandleError, this));
}

Connection::~Connection()
{
    DBG_LOG("Release connection:%p", this);
}
