#pragma once

#include "channel.h"
#include "poller.h"
#include "timeTask.h"
#include "timeWheel.h"

#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include<atomic>
#include<unistd.h>

class EventLoop
{
private:
    using Func = std::function<void()>;
    std::thread::id _thread_id; // 线程id
    int _event_fd;
    std::atomic_bool isStop{false};
    std::unique_ptr<Channel> _event_channel; // 事件fd的Channel对象
    Poller _poller;
    std::vector<Func> _tasks; // 任务池
    std::mutex _mutex;        // 保护任务池线程安全
    std::shared_ptr<TimerWheel> _timer_wheel;  // 时间轮定时器
public:
    void RunAllTask();
    // 对eventfd的操作封装
    static int CreatEventFd();
    void ReadEeventFd();
    void WakeUpEventFd();
    inline void stop()
    {
        isStop=true;
        WakeUpEventFd();
    }
    inline bool getIsStop()
    {
        return isStop.load();
    }
public:
    EventLoop();
    ~EventLoop(){close(_event_fd);}
    void Start();
    // 判断当前线程是否是EventLoop所对应的线程
    bool IsInLoopThread()
    {
        return _thread_id == std::this_thread::get_id();
    }
    // 保证当前线程就是要执行任务的线程
    void AssertInLoop()
    {
        assert(_thread_id == std::this_thread::get_id());
    }
    void RunInLoop(const Func &cb);
    void AddTasks(const Func &cb);
    // 对poller channel timerwheel的接口进行封装，供外部调用
    // 添加/修改描述符的事件监控
    void UpdataEvent(Channel *channel)
    {
        _poller.UpdataEvent(channel);
    }
    // 移除描述符的事件监控
    void RemoveEvent(Channel *channel)
    {
        _poller.RemoveEvent(channel);
    }
    void TimerAdd(uint64_t id, uint32_t delay, const Taskfunc &cb)
    {
        _timer_wheel->TimerAdd(id, delay, cb);
    }
    void TimerRefresh(uint64_t id)
    {
        _timer_wheel->TimerRefresh(id);
    }
    void TimerCancel(uint64_t id)
    {
        _timer_wheel->TimerCancel(id);
    }
    bool HasTimer(uint64_t id)
    {
        return _timer_wheel->HasTimer(id);
    }
};
