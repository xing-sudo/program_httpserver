#pragma once

#include"socket.h"
#include"channel.h"
#include"eventLoop.h"
#include<assert.h>


//对监听套接字的管理
class Acceptor
{
    private:
    Socket _socket;
    EventLoop* _loop;//事件监控
    Channel _channel;//事件管理

    using AcceptorCallBack=std::function<void(int)>;
    AcceptorCallBack _accept_callback;//监听套接字有连接请求回调
    private:
    void HandleRead();
    int CreateServer(int port);
    public:
    Acceptor(EventLoop* loop,int port);
    inline void SetAcceptorCallBack(const AcceptorCallBack& cb)
    {
        _accept_callback=cb;
    }
    inline void Listen()
    {
        _channel.EnableRead();
    }
};