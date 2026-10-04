#pragma once

#include"eventLoop.h"
#include<functional>
#include<memory>
#include <fcntl.h>
#include <errno.h>

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
    int _fd; // 监控的文件描述符
    std::weak_ptr<void> tie; // 监控对象的弱引用，防止监控对象被销毁后Channel还在访问它
    bool _istie=false; // 标记是否关闭
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
    Channel(EventLoop* loop,int fd):
    _fd(fd),
    _events(0),
    _revents(0),
    _loop(loop)
    {}
    void Tie(const std::shared_ptr<void> &obj)
    {
        tie = obj;
        _istie=true;
    } // 绑定监控对象
    int GetEvents() const
    {
        return _events;
    } // 获取监控的事件类型
    int GetFd() const
    {
        return _fd;
    } // 获取监控的文件描述符
    void SetRevents(uint32_t revents)
    {
        _revents = revents;
    } // 设置触发的事件类型
    void SetReadCallBack(const Eventcallback &cb)
    {
        _read_callback = cb;
    } // 设置可读事件回调函数
    void SetWriteCallBack(const Eventcallback &cb)
    {
        _write_callback = cb;
    } // 设置可写事件回调函数
    void SetCloseCallBack(const Eventcallback &cb)
    {
        _close_callback = cb;
    } // 设置关闭事件回调函数
    void SetErrorCallBack(const Eventcallback &cb)
    {
        _error_callback = cb;
    } // 设置错误事件回调函数
    void SetEventCallBack(const Eventcallback &cb)
    {
        _event_callback = cb;
    } // 设置任意事件回调函数
    void EnableRead()
    {
        _events |= EPOLLIN;
        Update();
    } // 开启可读事件监控
    void EnableWrite()
    {
        _events |= EPOLLOUT;
        Update();
    } // 开启可写事件监控
    void DisableRead() { 
        _events &= ~EPOLLIN; 
        Update();
    } // 关闭可读事件监控
    void DisableWrite()
    {
        _events &= ~EPOLLOUT;
        Update();
    } // 关闭可写事件监控
    void DisableAll()
    {
        _events = 0;
        Update();
    } // 关闭所有事件监控
    bool ReadAble()
    {
        return _events & EPOLLIN;
    } // 是否可读
    bool WriteAble()
    {
        return _events & EPOLLOUT;
    } // 是否可写
    void Channel::Remove()
{
    return _loop->RemoveEvent(this);
}
void Channel::Update()
{
    return _loop->UpdataEvent(this);
}

    void HandleEvent()
    {
        std::shared_ptr<void> obj ;
        if(_istie)
        {
            obj = tie.lock();
            if(!obj)return;
        }
        if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
        {
            // 可读事件触发，EPOLLRDHUP表示对端关闭连接，EPOLLPRI表示有紧急数据可读
            if (_read_callback)
            {
                _read_callback();
            }
        }
        if (_revents & EPOLLOUT)
        {
            if (_write_callback)
            {
                _write_callback();
            }
        }
         if (_revents & EPOLLHUP)
        {
            if (_close_callback)
            {
                _close_callback();
            }
        }
         if (_revents & EPOLLERR)
        {
            if (_error_callback)
            {
                _error_callback();
            }
        }
        // 无论什么事件都要触发任意事件回调函数，刷新活跃度
        if (_event_callback)
        {
            _event_callback();
        }
    } // 处理触发的事件
};