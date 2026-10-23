#pragma once

#include "channel.h"
#include "timeTask.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <unistd.h>

class EventLoop;

// timerfd_create函数用于创建一个新的定时器文件描述符，
// settimerfd函数用于设置定时器的初始值和间隔时间，
// itimerspec结构体定义如下：
// struct itimerspec {
//     struct timespec it_interval; // 定时器的间隔时间
//     struct timespec it_value;    // 定时器的初始值
// };
class TimerWheel : public std::enable_shared_from_this<TimerWheel>
{
private:
    struct TimeNode
    {
        uint64_t id;
        std::shared_ptr<Timetask> task;
    };
    struct TimePosition
    {
        uint32_t slot;
        uint32_t index;
    };
    using Slot=std::vector<TimeNode>;
    int _tick;     /// 秒针走到哪释放哪
    int _capacity; // 最大延迟时间
    EventLoop *_loop;
    int _timerfd;
    std::unordered_map<uint64_t, TimePosition> _timer; // 定时器id到定时器对象的映射
    std::vector<Slot> _wheel;    // 时间轮
    std::unique_ptr<Channel> _timer_channel;

private:
    void RemoveTimer(uint64_t id);
    static int Createtimerfd();
    int ReadTimerfd();
    void Run(int ticks);
    void OnTime();
    void executeTask();
    void TimerAddInLoop(uint64_t id, uint32_t delay, const Taskfunc &cb);
    void TimerRefreshInLoop(uint64_t id);
    void TimerCancelInLoop(uint64_t id);
    std::shared_ptr<Timetask> detachNode(uint64_t id);

public:
    TimerWheel(EventLoop *loop);
    ~TimerWheel()
    {
        for(auto&queue:_wheel)
        {
            for(auto &ptr:queue)
            {
                ptr.task->cancel();
            }
        }
        _wheel.clear();
        _timer.clear();
        close(_timerfd);
    }
    void TimerAdd(uint64_t id, uint32_t delay, const Taskfunc &cb);
    void TimerRefresh(uint64_t id);
    void TimerCancel(uint64_t id);
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
