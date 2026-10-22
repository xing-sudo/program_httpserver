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
    return static_cast<int>(times);
}

void TimerWheel::Run()
{
    _tick = (_tick + 1) % _capacity; // 秒针走一步
    auto& it=_wheel[_tick];
    std::vector<TimerWheel::ShPtrTask>tmp;
    std::erase_if(it,[&](TimerWheel::ShPtrTask& e ){
        if(e->getCount()>0)
        {
            e->decreaseCount();
            return false;
        }else{
            tmp.push_back(e);//先清空槽位，再处理任务
            _timer.erase(e->getId());//正在执行说明这个任务不可用了，就不要再刷新了
            return true;
        }
    });
    for(auto& task:tmp)
    {
        task->executeTask();
    }
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
    if(pt->isInExecute())
    {
        return ;
    }
    int delay = pt->Delaytime();
    pt->resetCount(delay);
    for(auto& slot:_wheel)//删除槽内旧的事件
    {
        std::erase_if(slot,[&](TimerWheel::ShPtrTask& task){
            if(task->getId()==id)
            {
                return true;
            }else{
                return false;
            }
        });
    }
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
    std::weak_ptr<TimerWheel> it=shared_from_this();
    _loop->RunInLoop([it,id,delay,cb](){
        if(auto ptr=it.lock())
        {
            ptr->TimerAddInLoop(id,delay,cb);
        }
    });
}

void TimerWheel::TimerCancel(uint64_t id)
{
    std::weak_ptr<TimerWheel> it=shared_from_this();
    _loop->RunInLoop([it,id](){
        if(auto ptr=it.lock())
        {
            ptr->TimerCancelInLoop(id);
        }
    });
}

void TimerWheel::TimerRefresh(uint64_t id)
{
    std::weak_ptr<TimerWheel> it=shared_from_this();
    _loop->RunInLoop([it,id](){
        if(auto ptr=it.lock())
        {
            ptr->TimerRefreshInLoop(id);
        }
    });
}
