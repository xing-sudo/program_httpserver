#include "channel.h"

#include "eventLoop.h"

void Channel::HandleEvent()
{
    std::shared_ptr<void> obj;
    if (_istie)
    {
        obj = tie.lock();
        if (!obj)
            return;
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
}

void Channel::Remove()
{
    return _loop->RemoveEvent(this);
}

void Channel::Update()
{
    return _loop->UpdataEvent(this);
}
