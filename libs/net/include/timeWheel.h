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
    using WeakTask = std::weak_ptr<Timetask>;
    using ShPtrTask = std::shared_ptr<Timetask>;
    int _tick;     /// 秒针走到哪释放哪
    int _capacity; // 最大延迟时间
    EventLoop *_loop;
    int _timerfd;
    std::unordered_map<uint64_t, WeakTask> _timer; // 定时器id到定时器对象的映射
    std::vector<std::vector<ShPtrTask>> _wheel;    // 时间轮
    std::unique_ptr<Channel> _timer_channel;

private:
    void RemoveTimer(uint64_t id);
    static int Createtimerfd();
    int ReadTimerfd();
    void Run();
    void OnTime();
    void TimerAddInLoop(uint64_t id, uint32_t delay, const Taskfunc &cb);
    void TimerRefreshInLoop(uint64_t id);
    void TimerCancelInLoop(uint64_t id);

public:
    TimerWheel(EventLoop *loop);
    ~TimerWheel()
    {
        for (auto &slot : _wheel)
        {
            for (auto &ptr : slot)
            {
                ptr->cancel(); // 取消所有定时任务
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
