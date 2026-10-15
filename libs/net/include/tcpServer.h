#pragma once

#include "acceptor.h"
#include "connection.h"
#include "eventLoop.h"
#include "loopThreadpool.h"

#include <cstdint>
#include <functional>
#include <unordered_map>

class TcpServer // 对所有模块整合使使用者能够轻松创建服务器
{
private:
    uint64_t _next_id;
    int _port;
    int _timeout;
    bool _enable_inactive_release;
    EventLoop _baseloop;   // 事件监控
    Acceptor _acceptor;    // 监听
    LoopThreadPool _pool;  // 线程池
    std::unordered_map<uint64_t, PtrConnection> _connections; // 连接管理

    using ConnectedCallBack = std::function<void(const PtrConnection &)>;
    using MessageCallBack = std::function<void(const PtrConnection &, Buffer *)>;
    using ClosedCallBack = std::function<void(const PtrConnection &)>;
    using AnyEventCallBack = std::function<void(const PtrConnection &)>;
    using Functor = std::function<void()>;
    ConnectedCallBack _connected_callback;
    MessageCallBack _message_callback;
    ClosedCallBack _istie_callback;
    AnyEventCallBack _any_event_callback;
private:
    void RunAfterInLoop(const Functor &cb, int delay);
    void NewConnection(int fd);
    void RemoveConnection(const PtrConnection &conn);
    void RemoveConnectionInLoop(const PtrConnection &conn);
public:
    TcpServer(int port);
    void SetThreadCount(int num)
    {
        return _pool.SetThreadCount(num);
    }
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
    void EnableInactiveRelease(int sec)
    {
        _timeout = sec;
        _enable_inactive_release = true;
    }
    void RunAfter(const Functor &cb, int delay);
    void Start();
    void stop();
};
