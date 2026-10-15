#include "timeWheel.h"

#include <sys/timerfd.h>
#include <unistd.h>

#include <cstdlib>
#include <functional>

#include "eventLoop.h"
#include "logger.h"

void TimerWheel::RemoveTimer(uint64_t id)
{
    auto it = _timer.find(id);
    if (it != _timer.end())
    {
        _timer.erase(it);
    }
}

int TimerWheel::Createtimerfd()
{
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timerfd < 0)
    {
        ERR_LOG("timerfd_create failed!!");
        abort();
    }
    struct itimerspec itime;
    itime.it_value.tv_sec = 1;
    itime.it_value.tv_nsec = 0;
    itime.it_interval.tv_sec = 1;
    itime.it_interval.tv_nsec = 0;
    timerfd_settime(timerfd, 0, &itime, nullptr);
    return timerfd;
}

int TimerWheel::ReadTimerfd()
{
    uint64_t times;
    int ret = read(_timerfd, &times, 8);
    if (ret < 0)
    {
        ERR_LOG("read timerfd failed!!");
        abort();
    }
    return (int)times;
}

void TimerWheel::Run()
{
    _tick = (_tick + 1) % _capacity; // 秒针走一步
    _wheel[_tick].clear();           // 释放该位置的所有定时器对象
}

void TimerWheel::OnTime()
{
    // 超时多少次执行多少次任务
    int count = ReadTimerfd();
    for (int i = 0; i < count; i++)
    {
        Run();
    }
}

void TimerWheel::TimerAddInLoop(uint64_t id, uint32_t delay, const Taskfunc &cb)
{
    ShPtrTask pt(new Timetask(id, delay, cb));
    pt->setDestroy(std::bind(&TimerWheel::RemoveTimer, this, id)); // 设置释放函数
    int pos = (_tick + delay) % _capacity;                         // 计算该定时器对象应该放到时间轮的哪个位置
    _wheel[pos].push_back(pt);                                     // 放入时间轮
    _timer[id] = WeakTask(pt);                                     // 存储弱引用
}

void TimerWheel::TimerRefreshInLoop(uint64_t id)
{
    // 通过保存的weak_ptr对象构造share_ptr对象放入轮子
    auto it = _timer.find(id);
    if (it == _timer.end())
    {
        return;
    }
    ShPtrTask pt = it->second.lock();
    if (!pt)
    {
        ERR_LOG("TimerRefreshInLoop weak_ptr is nullptr");
        return;
    }
    int delay = pt->Delaytime();
    int pos = (_tick + delay) % _capacity; // 计算该定时器对象应该放到时间轮的哪个位置
    _wheel[pos].push_back(pt);             // 放入时间轮
}

void TimerWheel::TimerCancelInLoop(uint64_t id)
{
    auto it = _timer.find(id);
    if (it == _timer.end())
    {
        return;
    }
    ShPtrTask pt = it->second.lock();
    if (pt)
        pt->cancel();
}

TimerWheel::TimerWheel(EventLoop *loop) : _tick(0), _capacity(60), _wheel(_capacity), _loop(loop),
                                          _timerfd(Createtimerfd()),
                                          _timer_channel(new Channel(_loop, _timerfd))
{
    _timer_channel->SetReadCallBack(std::bind(&TimerWheel::OnTime, this));
    _timer_channel->EnableRead();
}

void TimerWheel::TimerAdd(uint64_t id, uint32_t delay, const Taskfunc &cb)
{
    return _loop->RunInLoop(std::bind(&TimerWheel::TimerAddInLoop, this, id, delay, cb));
}

void TimerWheel::TimerCancel(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerCancelInLoop, this, id));
}

void TimerWheel::TimerRefresh(uint64_t id)
{
    return _loop->RunInLoop(std::bind(&TimerWheel::TimerRefreshInLoop, this, id));
}
