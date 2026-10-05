#pragma once
#include<functional>

using Taskfunc = std::function<void()>; // 定时任务
using Destroy = std::function<void()>;  // 定时器对象释放函数
class Timetask
{

private:
    uint64_t _id;      // 定时器任务id
    uint32_t _timeout; // 定时任务的推迟时间
    bool _cancel;      // 定时任务是否被取消
    Taskfunc _func;    // 定时器对象要执行的任务
    Destroy _Destroy;  // timewheel释放该定时器对象时要执行的函数
public:
    Timetask(uint64_t id, uint32_t delay, const Taskfunc &cb) : _id(id), _timeout(delay), _cancel(false), _func(cb) {}

    ~Timetask()
    {
        if (_cancel == false) // 定时任务没有被取消,则执行定时任务
        {
            _func();
        }
        _Destroy();
    }
    // 设置是否取消
    void cancel()
    {
        _cancel = true;
    }
    // 设置释放函数
    void setDestroy(const Destroy &rel)
    {
        _Destroy = rel;
    }
    // 获取定时任务推迟时间
    uint32_t Delaytime()
    {
        return _timeout;
    }
};