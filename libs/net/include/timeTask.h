#pragma once

#include <cstdint>
#include <functional>

using Taskfunc = std::function<void()>; // 定时任务
using Destroy = std::function<void()>;  // 定时器对象释放函数
class Timetask
{

private:
    uint64_t _id;         // 定时器任务id
    uint32_t _timeout;    // 定时任务的推迟时间
    size_t count;         // 圈数
    bool _cancel = false; // 定时任务是否被取消
    bool isExecute = false;
    bool isExecuting=false;
    Taskfunc _func;   // 定时器对象要执行的任务
    Destroy _Destroy; // timewheel释放该定时器对象时要执行的函数
public:
    Timetask(uint64_t id, uint32_t delay, const Taskfunc &cb) : _id(id), _timeout(delay), _cancel(false), _func(cb),
                                                                count(delay == 0 ? 0 : (delay - 1) / 60) {}

    ~Timetask()
    {
        if(_Destroy)
        _Destroy();
    }
    inline void executeTask()
    {
        if (_cancel || isExecute) //// 定时任务没有被取消并且没有被执行过,则执行定时任务
        {
            return;
        }
        isExecute = true;
        isExecuting=true;
        if (_func)
            _func();
    }
    inline bool isInExecute()
    {
        return isExecuting;
    }
    inline void resetCount(int delay)
    {
        count = (delay == 0 ? 0 : (delay - 1) / 60);
    }
    inline size_t getCount()
    {
        return count;
    }
    inline uint64_t getId()
    {
        return _id;
    }
    inline void decreaseCount()
    {
        count--;
        return;
    }
    // 设置是否取消
    inline void cancel()
    {
        _cancel = true;
    }
    // 设置释放函数
    inline void setDestroy(const Destroy &rel)
    {
        _Destroy = rel;
    }
    // 获取定时任务推迟时间
    inline uint32_t Delaytime()
    {
        return _timeout;
    }
};
