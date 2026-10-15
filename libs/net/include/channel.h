#pragma once

#include <sys/epoll.h>

#include <cstdint>
#include <functional>
#include <memory>

class EventLoop;
// 对描述符需要监控的事件和触发的事件进行管理(可读，可写,关闭，错误，任意)
class Channel
{
    /*
    EPOLLIN：表示对应的文件描述符可以读（包括对端SOCKET正常关闭）；
    EPOLLOUT：表示对应的文件描述符可以写；
    EPOLLRDHUP：表示对应的文件描述符读关闭；
    EPOLLPRI：表示对应的文件描述符有紧急数据可读；
    EPOLLHUP：表示对应的文件描述符被挂断；
    EPOLLERR：表示对应的文件描述符发生错误；
    */
private:
    int _fd;                 // 监控的文件描述符
    std::weak_ptr<void> tie; // 监控对象的弱引用，防止监控对象被销毁后Channel还在访问它
    bool _istie = false;     // 标记是否关闭
    EventLoop *_loop;
    uint32_t _events;  // 监控的事件类型
    uint32_t _revents; // 触发的事件类型
    using Eventcallback = std::function<void()>;
    Eventcallback _read_callback;  // 可读事件回调函数
    Eventcallback _write_callback; // 可写事件回调函数
    Eventcallback _close_callback; // 关闭事件回调函数
    Eventcallback _error_callback; // 错误事件回调函数
    Eventcallback _event_callback; // 任意事件回调函数
public:
    Channel(EventLoop *loop, int fd) : _fd(fd),
                                       _events(0),
                                       _revents(0),
                                       _loop(loop)
    {
    }
    void HandleEvent();
    void Remove(); // 移除监控
    void Update(); // 更新监控事件
    inline void Tie(const std::shared_ptr<void> &obj)
    {
        tie = obj;
        _istie = true;
    } // 绑定监控对象
    inline int GetEvents() const
    {
        return _events;
    } // 获取监控的事件类型
    inline int GetFd() const
    {
        return _fd;
    } // 获取监控的文件描述符
    inline void SetRevents(uint32_t revents)
    {
        _revents = revents;
    } // 设置触发的事件类型
    inline void SetReadCallBack(const Eventcallback &cb)
    {
        _read_callback = cb;
    } // 设置可读事件回调函数
    inline void SetWriteCallBack(const Eventcallback &cb)
    {
        _write_callback = cb;
    } // 设置可写事件回调函数
    inline void SetCloseCallBack(const Eventcallback &cb)
    {
        _close_callback = cb;
    } // 设置关闭事件回调函数
    inline void SetErrorCallBack(const Eventcallback &cb)
    {
        _error_callback = cb;
    } // 设置错误事件回调函数
    inline void SetEventCallBack(const Eventcallback &cb)
    {
        _event_callback = cb;
    } // 设置任意事件回调函数
    inline void EnableRead()
    {
        _events |= EPOLLIN;
        Update();
    } // 开启可读事件监控
    inline void EnableWrite()
    {
        _events |= EPOLLOUT;
        Update();
    } // 开启可写事件监控
    inline void DisableRead()
    {
        _events &= ~EPOLLIN;
        Update();
    } // 关闭可读事件监控
    inline void DisableWrite()
    {
        _events &= ~EPOLLOUT;
        Update();
    } // 关闭可写事件监控
    inline void DisableAll()
    {
        _events = 0;
        Update();
    } // 关闭所有事件监控
    inline bool ReadAble()
    {
        return _events & EPOLLIN;
    } // 是否可读
    inline bool WriteAble()
    {
        return _events & EPOLLOUT;
    } // 是否可写
};
