#pragma once

#include "any.h"
#include "buffer.h"
#include "channel.h"
#include "eventLoop.h"
#include "socket.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

class Connection;
// 连接管理：套接字，丢包粘包，协议解析，连接上下文
// DISCONNECTED：连接关闭事件,CONNECTING：连接建立成功，待处理
// CONNECTED:连接建立成功，各种设置完成，可以通信；DISCONNECTING:连接待关闭
typedef enum
{
    DISCONNECTED, // 连接关闭
    CONNECTING,   // 连接建立成功，待处理
    CONNECTED,    // 连接建立成功，各种设置完成，可以通信
    DISCONNECTING // 连接待关闭
} ConnStatus;
using PtrConnection = std::shared_ptr<Connection>;
class Connection : public std::enable_shared_from_this<Connection> // 用智能指针指向自己防止其他地方将connection对象释放，导致内存访问错误
{
private:
    uint64_t _connection_id;       // 连接id
    int _sockfd;                   // 连接套接字
    bool _enable_inactive_release; // 是否开启连接不活跃释放
    EventLoop *_loop;              // 连接所属的EventLoop
    ConnStatus _status;            // 连接状态
    Socket _socket;                // 连接套接字封装
    Channel _channel;              // 连接的事件管理
    Buffer _inbuffer;              // 连接的输入缓冲区
    Buffer _outbuffer;             // 连接的输出缓冲区
    Any _context;                  // 连接上下文
    // 四个回调由组件使用者决定
    using ConnectedCallBack = std::function<void(const PtrConnection &)>;
    using MessageCallBack = std::function<void(const PtrConnection &, Buffer *)>;
    using ClosedCallBack = std::function<void(const PtrConnection &)>;
    using AnyEventCallBack = std::function<void(const PtrConnection &)>; // 任意事件回调函数，触发时会传递连接对象和上下文对象

    ConnectedCallBack _connected_callback; // 连接建立成功回调函数
    MessageCallBack _message_callback;     // 消息到达回调函数
    ClosedCallBack _istie_callback;        // 连接关闭回调函数
    AnyEventCallBack _any_event_callback;  // 任意事件回调函数
    ClosedCallBack _server_istie_callback; // 服务器关闭回调函数
private:
    // 五个channel事件回调
    void HandleRead();  // 读
    void HandleWrite(); // 写
    void HandleClose(); // 关闭
    void HandleAnyEvent();
    void HandleError();
    // 连接获取之后要对各种属性进行设置才可以通信，即connecting->connected
    void EstablishedInLoop();
    // 实际的释放接口
    void ReleaseInLoop();
    // 不是实际的发送接口只是将数据放到了缓冲区，并开启写监控
    void SendInLoop(Buffer buf);
    void ShutdownInLoop();
    void EnableInactiveReleaseInLoop(int sec);
    void CancelInactiveReleaseInLoop();
    void UpgradeInLoop(const Any &context, const ConnectedCallBack &con,
                       const MessageCallBack &msg, const ClosedCallBack &closed,
                       const AnyEventCallBack &any);

public:
    Connection(EventLoop *loop, uint64_t id, int sockfd);
    ~Connection();
    int Fd()
    {
        return _sockfd;
    }
    // 获取连接id
    int Id()
    {
        return _connection_id;
    }
    // 是否为CONNECTED状态
    bool Connected()
    {
        return (_status == CONNECTED);
    }
    // 设置上下文
    void SetContext(const Any &context)
    {
        _context = context;
    }
    // 获取上下文
    Any *GetContext()
    {
        return &_context;
    }
    // 设置各种回调
    void SetConnectedCallBack(const ConnectedCallBack &cb)
    {
        _connected_callback = cb;
    }
    void SetMessageCallBack(const MessageCallBack &cb)
    {
        _message_callback = cb;
    }
    void SetClosedCallBack(const ClosedCallBack &cb)
    {
        _istie_callback = cb;
    }
    void SetAnyEventCallBack(const AnyEventCallBack &cb)
    {
        _any_event_callback = cb;
    }
    void SetServerCloseCallBack(const ClosedCallBack &cb)
    {
        _server_istie_callback = cb;
    }
    // 连接建立完毕，进行channel的各种回调设置，启动读监控，调用connected_callback
    void Established()
    {
        _loop->RunInLoop([it = shared_from_this()]()
                         { it->EstablishedInLoop(); });
    }
    void Send(const char *data, size_t len)
    {
        // 由于外界传入的data有可能是临时空间所以先将data的数据取出放入缓冲区进行操作
        Buffer tmp;
        tmp.Write(data, len);
        _loop->RunInLoop([it = weak_from_this(), buf = std::move(tmp)]() mutable
                         {
            if(auto ptr=it.lock())
            {
                if(ptr->_status==DISCONNECTED)
                return;
                ptr->SendInLoop(std::move(buf));
            } });
        // C++14的lambda能够对捕获进来的变量进行改名和执行表达式和直接初始化
        // std::bind传输的是左值，lambda可以完全替代它
    }
    void Shutdown()
    {
        _loop->RunInLoop([it = weak_from_this()]()
                         {
            if(auto ptr=it.lock())
            {
                if(ptr->_status==DISCONNECTED)
                return;
                ptr->ShutdownInLoop();
            } });
    }
    void Release()
    {
        _loop->RunInLoop([it = weak_from_this()]()
                         {
            if(auto ptr=it.lock())
            {
                if(ptr->_status==DISCONNECTED)
                return;
                ptr->ReleaseInLoop();
            } });
    }
    void EnableInactiveRelease(int sec)
    {
        _loop->RunInLoop([it = weak_from_this(), sec]()
                         {
            if(auto ptr=it.lock())
            {
                if(ptr->_status==DISCONNECTED)
                return;
                ptr->EnableInactiveReleaseInLoop(sec);
            } });
    }
    void CancelInactiveRelease()
    {
        _loop->RunInLoop([it = weak_from_this()]()
                         {
            if(auto ptr=it.lock())
            {
                if(ptr->_status==DISCONNECTED)
                return;
                ptr->CancelInactiveReleaseInLoop();
            } });
    }
    void Upgrade(const Any &context, const ConnectedCallBack &conn, const MessageCallBack &msg,
                 const ClosedCallBack &closed, const AnyEventCallBack &event)
    {
        // 切换上下文必须在对应的EventLoop线程中立即执行防止新的事件触发后使用的还是原上下文
        _loop->AssertInLoop();
        _loop->RunInLoop([it = weak_from_this(), context, conn, msg, closed, event]()
                         {
            if(auto ptr=it.lock())
            {
                if(ptr->_status==DISCONNECTED)
                return;
                ptr->UpgradeInLoop(context,conn,msg,closed,event);
            } });
    }
};
