#pragma once

#include"buffer.h"
#include"socket.h"
#include"any.h"
#include"channel.h"
#include"eventLoop.h"

//连接管理：套接字，丢包粘包，协议解析，连接上下文
//DISCONNECTED：连接关闭事件,CONNECTING：连接建立成功，待处理
//CONNECTED:连接建立成功，各种设置完成，可以通信；DISCONNECTING:连接待关闭
typedef enum {
    DISCONNECTED,//连接关闭
    CONNECTING,//连接建立成功，待处理
    CONNECTED,//连接建立成功，各种设置完成，可以通信
    DISCONNECTING//连接待关闭
}ConnStatus;
using PtrConnection=std::shared_ptr<Connection>;
class Connection:public std::enable_shared_from_this<Connection>//用智能指针指向自己防止其他地方将connection对象释放，导致内存访问错误
{
    private:
    uint64_t _connection_id;//连接id
    int _sockfd; //连接套接字
    bool _enable_inactive_release;//是否开启连接不活跃释放
    EventLoop* _loop;//连接所属的EventLoop
    ConnStatus _status;//连接状态
    Socket _socket;//连接套接字封装
    Channel _channel;//连接的事件管理
    Buffer _inbuffer;//连接的输入缓冲区
    Buffer _outbuffer;//连接的输出缓冲区
    Any _context;//连接上下文
    //四个回调由组件使用者决定
    using ConnectedCallBack=std::function<void(const PtrConnection&)>;
    using MessageCallBack=std::function<void(const PtrConnection&,Buffer*)>;
    using ClosedCallBack=std::function<void(const PtrConnection&)>;
    using AnyEventCallBack=std::function<void(const PtrConnection&)>;//任意事件回调函数，触发时会传递连接对象和上下文对象

    ConnectedCallBack _connected_callback;//连接建立成功回调函数
    MessageCallBack _message_callback;//消息到达回调函数
    ClosedCallBack _istie_callback;//连接关闭回调函数
    AnyEventCallBack _any_event_callback;//任意事件回调函数
    ClosedCallBack _server_istie_callback;//服务器关闭回调函数
    private:
    //五个channel事件回调
    void HandleRead()//读
    {
        //1.接收socket数据，放到输入缓冲区
        char buff[65535];
        ssize_t ret=_socket.NonBlockRecv(buff,sizeof(buff));
        if(ret<0)
        {
            //出错不能直接关闭连接，进一步判断buff状态
            return ShutdownInLoop();
        }
        //将数据放到我们的输入缓冲区
        _inbuffer.Write(buff,ret);
        //2.调用消息回调函数，处理输入缓冲区数据
        if(_inbuffer.ReadAbleSize()>0)
        {
            //shared_from_this()获取当前对象的shared_ptr智能指针
            _message_callback(shared_from_this(),&_inbuffer);
        }
    }
    void HandleWrite()
    {
        //outbuffer中就是要发送的数据
        ssize_t ret=_socket.NonBlockSend(_outbuffer.GetReadPos(),_outbuffer.ReadAbleSize());
        if(ret<0)
        {
            //发送错误关闭连接，但要判断buffer中是否还有数据
            if(_inbuffer.ReadAbleSize()>0)
            {
                //如果输入缓冲区还有数据，说明连接还活跃，不应该直接关闭连接，而是调用消息回调函数处理输入缓冲区中的数据
                _message_callback(shared_from_this(),&_inbuffer);;
            }
            return Release();
        }
        _outbuffer.MoveReadPos(ret);
         if(_outbuffer.ReadAbleSize()==0){
            _channel.DisableWrite();//没有数据待发送了关闭写监控
            if(_status==DISCONNECTING)
            {
                //如果连接状态是待关闭,说明之前是因为还有数据没有处理导致没有关闭连接
                return Release();
            }
         }
         return;
    }
    void HandleClose()
    {
        //连接关闭，套接字什么都不干了判断后直接关闭
        if(_inbuffer.ReadAbleSize()>0)
        {
            _message_callback(shared_from_this(),&_inbuffer);
        }
        return Release();
    }
    void HandleAnyEvent()
    {
        //触发任何事件都刷新活跃度，调用使用者设置的任意事件回调
        if(_enable_inactive_release==true){
            _loop->TimerRefresh(_connection_id);
        }
        if(_any_event_callback){
            _any_event_callback(shared_from_this());
        }
    }
    void HandleError()
    {
        return HandleClose();//直接复用close
    }
    //连接获取之后要对各种属性进行设置才可以通信，即connecting->connected
    void EstablishedInLoop()
    {
        //1.改状态
        assert(_status==CONNECTING);
        _status=CONNECTED;
        //2.启动读监控
        _channel.EnableRead();
        _channel.Tie(shared_from_this());//绑定监控对象，防止连接对象被销毁后channel还在访问它
        //3.调用用户设置的回调
        if(_connected_callback)
        {
            _connected_callback(shared_from_this());
        }
    }
    //实际的释放接口
    void ReleaseInLoop()
    {
        //1.修改连接状态
        _status=DISCONNECTED;
        //2.移除事件监控
        _channel.Remove();
        //3.关闭描述符
        _socket.Close();
        //4.如果当前连接在定时器队列中还有定时销毁任务则取消
        if(_loop->HasTimer(_connection_id))
        {
            CancelInactiveReleaseInLoop();
        }
        //5.调用关闭回调，避免先调用服务器回调导致连接释放，后续操作非法
        if(_istie_callback)
        {
            _istie_callback(shared_from_this());
        }
        //移除服务器内部管理信息
        if(_server_istie_callback)
        {
            _server_istie_callback(shared_from_this());
        }
    }
    //不是实际的发送接口只是将数据放到了缓冲区，并开启写监控
    void SendInLoop(Buffer &buf)
    {
        if(_status==DISCONNECTED)
        {
            return;
        }
        _outbuffer.Write(buf); 
        if(_channel.WriteAble()==false)
        {
            _channel.EnableWrite();
        }
    }
    void ShutdownInLoop()
    {
        //设置待关闭
        _status=DISCONNECTING;
        //如果输入缓冲区有数据就读取
        if(_inbuffer.ReadAbleSize()>0){
            if(_message_callback){
                _message_callback(shared_from_this(),&_inbuffer);
            }
        }
        //如果输出缓冲区有数据就发送
        if(_outbuffer.ReadAbleSize()>0)
        {
            if(_channel.WriteAble()==false)
            {
                _channel.EnableWrite();
            }
        }
        //所有都处理完了就关闭
        if(_outbuffer.ReadAbleSize()==0)
        {
            Release();
        }
    }

    void EnableInactiveReleaseInLoop(int sec)
    {
        //更改标志位
        _enable_inactive_release=true;
        //判断是否有定时销毁任务有就刷新
        if(_loop->HasTimer(_connection_id))
        {
            return _loop->TimerRefresh(_connection_id);
        }
        //没有则添加
        _loop->TimerAdd(_connection_id,sec,std::bind(&Connection::Release,this));
    }
    void CancelInactiveReleaseInLoop()
    {
        //更改标志位
        _enable_inactive_release=false;
        //有定时销毁就取消
        if(_loop->HasTimer(_connection_id))
        {
            _loop->TimerCancel(_connection_id);
        }
    }
    void UpgradeInLoop(const Any &context,const ConnectedCallBack &con,
                        const MessageCallBack& msg,const ClosedCallBack &closed,
                        const AnyEventCallBack & any)
    {
        _context=context;
        _connected_callback=con;
        _message_callback=msg;
        _istie_callback=closed;
        _any_event_callback=any;
    }
    public:
    Connection(EventLoop* loop,uint64_t id,int sockfd):_connection_id(id),_sockfd(sockfd),_loop(loop),
    _enable_inactive_release(false),_status(CONNECTING),_socket(_sockfd),_channel(loop,sockfd)
    {
        _channel.SetReadCallBack(std::bind(&Connection::HandleRead,this));
        _channel.SetWriteCallBack(std::bind(&Connection::HandleWrite,this));
        _channel.SetCloseCallBack(std::bind(&Connection::HandleClose,this));
        _channel.SetEventCallBack(std::bind(&Connection::HandleAnyEvent,this));
        _channel.SetErrorCallBack(std::bind(&Connection::HandleError,this));
    }
    ~Connection()
    {
        DBG_LOG("Release connection:%p",this);
    }
    int Fd()
    {
        return _sockfd;
    }
    //获取连接id
    int Id()
    {
        return _connection_id;
    }
    //是否为CONNECTED状态
    bool Connected()
    {
        return (_status==CONNECTED);
    }
    //设置上下文
    void SetContext(const Any& context)
    {
        _context=context;
    }
    //获取上下文
    Any* GetContext()
    {
        return &_context;
    }
    //设置各种回调
    void SetConnectedCallBack(const ConnectedCallBack& cb)
    {
        _connected_callback=cb;
    }
    void SetMessageCallBack(const MessageCallBack& cb)
    {
        _message_callback=cb;
    }
    void SetClosedCallBack(const ClosedCallBack& cb)
    {
        _istie_callback=cb;
    }
    void SetAnyEventCallBack(const AnyEventCallBack& cb)
    {
        _any_event_callback=cb;
    }
    void SetServerCloseCallBack(const ClosedCallBack& cb)
    {
        _server_istie_callback=cb;
    }
    //连接建立完毕，进行channel的各种回调设置，启动读监控，调用connected_callback
    void Established()
    {
        _loop->RunInLoop(std::bind(&Connection::EstablishedInLoop,this));
    }
    void Send(const char* data,size_t len)
    {
        //由于外界传入的data有可能是临时空间所以先将data的数据取出放入缓冲区进行操作
        Buffer tmp;
        tmp.Write(data,len);
        _loop->RunInLoop(std::bind(&Connection::SendInLoop,this,std::move(tmp)));
    }
    void Shutdown()
    {
        _loop->RunInLoop(std::bind(&Connection::ShutdownInLoop,this));
    }
    void Release()
    {
        _loop->RunInLoop(std::bind(&Connection::ReleaseInLoop,this));
    }
    void EnableInactiveRelease(int sec)
    {
        _loop->RunInLoop(std::bind(&Connection::EnableInactiveReleaseInLoop,this,sec));
    }
    void CancelInactiveRelease()
    {
        _loop->RunInLoop(std::bind(&Connection::CancelInactiveReleaseInLoop,this));
    }
    void Upgrade(const Any &context, const ConnectedCallBack &conn, const MessageCallBack &msg, 
                const ClosedCallBack &closed, const AnyEventCallBack &event)
        {
            //切换上下文必须在对应的EventLoop线程中立即执行防止新的事件触发后使用的还是原上下文
            _loop->AssertInLoop();
            _loop->RunInLoop(std::bind(&Connection::UpgradeInLoop,this,context,conn,msg,closed,event));
        }
};