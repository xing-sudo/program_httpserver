#include "tcpServer.h"

#include <functional>
#include <utility>

void TcpServer::RunAfterInLoop(const Functor &cb, int delay)
{
    _next_id++;
    _baseloop.TimerAdd(_next_id, delay, cb);
}

void TcpServer::NewConnection(int fd)
{
    _next_id++;
    PtrConnection conn(new Connection(_pool.NextLoop(), _next_id, fd));
    conn->SetConnectedCallBack(_connected_callback);
    conn->SetMessageCallBack(_message_callback);
    conn->SetClosedCallBack(_istie_callback);
    conn->SetAnyEventCallBack(_any_event_callback);
    conn->SetServerCloseCallBack(std::bind(&TcpServer::RemoveConnection, this, std::placeholders::_1));
    if (_enable_inactive_release)
    {
        conn->EnableInactiveRelease(_timeout);
    }
    conn->Established();
    _connections.insert(std::make_pair(_next_id, conn));
}

void TcpServer::RemoveConnection(const PtrConnection &conn)
{
    _baseloop.RunInLoop(std::bind(&TcpServer::RemoveConnectionInLoop, this, conn));
}

void TcpServer::RemoveConnectionInLoop(const PtrConnection &conn)
{
    int id = conn->Id();
    auto it = _connections.find(id);
    if (it != _connections.end())
    {
        _connections.erase(it);
    }
}

TcpServer::TcpServer(int port) : _port(port),
                                 _next_id(0),
                                 _enable_inactive_release(false),
                                 _acceptor(&_baseloop, port),
                                 _pool(&_baseloop)
{
    _acceptor.SetAcceptorCallBack(std::bind(&TcpServer::NewConnection, this, std::placeholders::_1));
    _acceptor.Listen();
}

void TcpServer::RunAfter(const Functor &cb, int delay)
{
    // 添加定时任务
    _baseloop.RunInLoop(std::bind(&TcpServer::RunAfterInLoop, this, cb, delay));
}

void TcpServer::Start()
{
    _pool.Create();
    _baseloop.Start();
}
void TcpServer::stop()
{
    _baseloop.stop();
    _pool.stop();
}
