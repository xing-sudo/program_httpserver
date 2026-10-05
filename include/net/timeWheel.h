#pragma once
#include"timeTask.h"
#include"logger.h"
#include"eventLoop.h"
#include <sys/timerfd.h>


//timerfd_create函数用于创建一个新的定时器文件描述符，
//settimerfd函数用于设置定时器的初始值和间隔时间，
//itimerspec结构体定义如下：
// struct itimerspec {
//     struct timespec it_interval; // 定时器的间隔时间
//     struct timespec it_value;    // 定时器的初始值
// };
class TimerWheel
{
private:
    using WeakTask = std::weak_ptr<Timetask>;
    using ShPtrTask = std::shared_ptr<Timetask>;
    int _tick;                                     /// 秒针走到哪释放哪
    int _capacity;                                 // 最大延迟时间
    std::vector<std::vector<ShPtrTask>> _wheel;    // 时间轮
    std::unordered_map<uint64_t, WeakTask> _timer; // 定时器id到定时器对象的映射

    EventLoop *_loop;
    int _timerfd; //
    std::unique_ptr<Channel> _timer_channel;

private:
    void RemoveTimer(uint64_t id)
    {
        auto it = _timer.find(id);
        if (it != _timer.end())
        {
            _timer.erase(it);
        }
    }
    static int Createtimerfd()
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
    int ReadTimerfd()
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
    void Run()
    {
        _tick = (_tick + 1) % _capacity; // 秒针走一步
        _wheel[_tick].clear();           // 释放该位置的所有定时器对象
    }
    void OnTime()
    {
        // 超时多少次执行多少次任务
        int count = ReadTimerfd();
        for (int i = 0; i < count; i++)
        {
            Run();
        }
    }
    void TimerAddInLoop(uint64_t id, uint32_t delay, const Taskfunc &cb)
    {
        ShPtrTask pt(new Timetask(id, delay, cb));
        pt->setDestroy(std::bind(&TimerWheel::RemoveTimer, this, id)); // 设置释放函数
        int pos = (_tick + delay) % _capacity;                        // 计算该定时器对象应该放到时间轮的哪个位置
        _wheel[pos].push_back(pt);                                    // 放入时间轮
        _timer[id] = WeakTask(pt);                                    // 存储弱引用
    }
    void TimerRefreshInLoop(uint64_t id)
    {
        //通过保存的weak_ptr对象构造share_ptr对象放入轮子
        auto it = _timer.find(id);
        if (it == _timer.end())
        {
            return;
        }
        ShPtrTask pt = it->second.lock();
        if(!pt)
        {
            ERR_LOG("TimerRefreshInLoop weak_ptr is nullptr");
        }
        int delay = pt->Delaytime();
        int pos = (_tick + delay) % _capacity; // 计算该定时器对象应该放到时间轮的哪个位置
        _wheel[pos].push_back(pt);             // 放入时间轮
    }
        void TimerCancelInLoop(uint64_t id)
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
public:
    TimerWheel(EventLoop* loop) : _tick(0), _capacity(60), _wheel(_capacity),_loop(loop),
        _timerfd(Createtimerfd()),
        _timer_channel(new Channel(_loop, _timerfd))
        {
        _timer_channel->SetReadCallBack(std::bind(&TimerWheel::OnTime, this));
        _timer_channel->EnableRead();
        
        }
void TimerWheel::TimerAdd(uint64_t id,uint32_t timeout,const Taskfunc &cb)
{
    return _loop->RunInLoop(std::bind(&TimerWheel::TimerAddInLoop,this,id,timeout,cb));
}
void TimerWheel::TimerCancel(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerCancelInLoop,this,id));
}
void TimerWheel::TimerRefresh(uint64_t id)
{
    return _loop->RunInLoop(std::bind(&TimerWheel::TimerRefreshInLoop,this,id));
}

    bool HasTimer(uint64_t id)
    {
        auto it = _timer.find(id);
        if (it == _timer.end())
        {
            return false;
        }
        return true;
    }
};