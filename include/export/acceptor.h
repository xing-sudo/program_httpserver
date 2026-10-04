#pragma once

#include"socket.h"
#include"channel.h"
#include"eventLoop.h"



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
    void HandleRead()
    {
        int newfd=_socket.Accept();
        if(newfd<0)
        {
            return;
        }
        if(_accept_callback){
            _accept_callback(newfd);
        }
    }
    int CreateServer(int port)
    {
        bool ret=_socket.CreateServer(port);
        assert(ret==true);
        return _socket.GetFd();
    }
    public:
    Acceptor(EventLoop* loop,int port):_loop(loop),_socket(CreateServer(port)),_channel(loop,_socket.GetFd())
    {
        //不能将启动读事件监控放到构造函数中
        //防止回调还没设置完成就有事件触发
        _channel.SetReadCallBack(std::bind(&Acceptor::HandleRead,this));
    }
    void SetAcceptorCallBack(const AcceptorCallBack& cb)
    {
        _accept_callback=cb;
    }
    void Listen()
    {
        _channel.EnableRead();
    }
};