#include "eventLoop.h"

#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <functional>
#include <vector>

#include "logger.h"

void EventLoop::RunAllTask()
{
    // 使用交换进行执行任务
    std::vector<Func> tmp;
    {
        std::unique_lock<std::mutex> _lock(_mutex);
        tmp.swap(_tasks);
    }
    for (auto &e : tmp)
    {
        e();
    }
    return;
}

int EventLoop::CreatEventFd()
{
    int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (efd < 0)
    {
        ERR_LOG("create eventfd failed!!");
        abort();
    }
    return efd;
}

void EventLoop::ReadEeventFd()
{
    uint64_t val = 0;
    ssize_t res = read(_event_fd, &val, 8);
    if (res < 0)
    {
        if (errno == EAGAIN || errno == EINTR)
        {
            return;
        }
        ERR_LOG("read eventfd failed!!");
        abort();
    }
    return;
}

void EventLoop::WakeUpEventFd()
{
    uint64_t val = 1;
    ssize_t res = write(_event_fd, &val, sizeof(val));
    if (res < 0)
    {
        if (errno == EINTR)
        {
            return;
        }
        ERR_LOG("write eventfd failed!!");
        abort();
    }
    return;
}

EventLoop::EventLoop() : _thread_id(std::this_thread::get_id()),
                         _event_fd(CreatEventFd()),
                         _event_channel(new Channel(this, _event_fd)),
                         _timer_wheel(std::make_shared<TimerWheel>(this))
{
    // 给eventfd设置读回调 读取通知次数 类内调用成员函数要传递this指针
    _event_channel->SetReadCallBack(std::bind(&EventLoop::ReadEeventFd, this));
    _event_channel->EnableRead();
}

void EventLoop::Start()
{
    // 1.监控 2.就绪处理 3.执行任务
    while (1)
    {
        if(isStop)
        {
            DBG_LOG("EventLoop: thread id [%lu] will stop",static_cast<uint64_t>(pthread_self()));
            return ;
        }
        // 1.
        std::vector<Channel *> active;
        _poller.Moniter(&active); // 接口是接口，调用者自己会对epfd进行updata
        // 2.
        for (auto &e : active)
        {
            e->HandleEvent();
        }
        // 3.
        RunAllTask();
    }
}

void EventLoop::RunInLoop(const Func &cb)
{
    // 判断当前线程和EventLoop所对应的线程是否相同，如果相同直接执行，否则加入任务池
    if (IsInLoopThread())
    {
        return cb();
    }
    return AddTasks(cb);
}

void EventLoop::AddTasks(const Func &cb)
{
    {
        std::unique_lock<std::mutex> _lock(_mutex);
        _tasks.push_back(cb);
    }
    // 唤醒EventLoop所在的线程，让其执行任务
    // 通过eventfd进行线程间通知
    // 防止因为eventfd没有就绪导致epoll阻塞
    WakeUpEventFd();
}
