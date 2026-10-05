#pragma once

#include"acceptor.h"
#include"loopThreadpool.h"
#include"connection.h"
#include"eventLoop.h"
#include<unordered_map>
#include<functional>




class TcpServer//对所有模块整合使使用者能够轻松创建服务器
{
    private:
    uint64_t _next_id;
    int _port;
    int _timeout;
    bool _enable_inactive_release;
    Acceptor _acceptor;//监听
    EventLoop _baseloop;//事件监控
    LoopThreadPool _pool;//线程池
    std::unordered_map<uint64_t,PtrConnection> _connections;//连接管理
    
    using ConnectedCallBack=std::function<void(const PtrConnection&)>;
    using MessageCallBack=std::function<void(const PtrConnection&,Buffer*)>;
    using ClosedCallBack=std::function<void(const PtrConnection&)>;
    using AnyEventCallBack=std::function<void(const PtrConnection&)>;
    using Functor=std::function<void()>;
    ConnectedCallBack _connected_callback;
    MessageCallBack _message_callback;
    ClosedCallBack _istie_callback;
    AnyEventCallBack _any_event_callback;
    private:
    void RunAfterInLoop(const Functor& cb,int delay)
    {
        _next_id++;
        _baseloop.TimerAdd(_next_id,delay,cb);
    }
    void NewConnection(int fd){
        _next_id++;
        PtrConnection conn(new Connection(_pool.NextLoop(),_next_id,fd));
        conn->SetConnectedCallBack(_connected_callback);
        conn->SetMessageCallBack(_message_callback);
        conn->SetClosedCallBack(_istie_callback);
        conn->SetAnyEventCallBack(_any_event_callback);
        conn->SetServerCloseCallBack(std::bind(&TcpServer::RemoveConnection,this,std::placeholders::_1));
        if(_enable_inactive_release)
        {
            conn->EnableInactiveRelease(_timeout);
        }
        conn->Established();
        _connections.insert(std::make_pair(_next_id,conn));
    }
    void RemoveConnection(const PtrConnection& conn)
    {
        _baseloop.RunInLoop(std::bind(&TcpServer::RemoveConnectionInLoop,this,conn));
    }
    void RemoveConnectionInLoop(const PtrConnection& conn)
    {
        int id=conn->Id();
        auto it=_connections.find(id);
        if( it !=_connections.end()){
        _connections.erase(it);
        }
    }
    public:
    TcpServer(int port):
    _port(port),
    _next_id(0),
    _enable_inactive_release(false),
    _acceptor(&_baseloop,port),
    _pool(&_baseloop)
    {
        _acceptor.SetAcceptorCallBack(std::bind(&TcpServer::NewConnection,this,std::placeholders::_1));
        _acceptor.Listen();
    }
    void SetThreadCount(int num)
    {
        return _pool.SetThreadCount(num);
    }
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
    void EnableInactiveRelease(int sec)
    {
        _timeout=sec;
        _enable_inactive_release=true;
    }
    void RunAfter(const Functor& cb,int delay)
    {
        //添加定时任务
        _baseloop.RunInLoop(std::bind(&TcpServer::RunAfterInLoop,this,cb,delay));
    }
    void Start()
    {
        _pool.Create();
        _baseloop.Start();
    }
};