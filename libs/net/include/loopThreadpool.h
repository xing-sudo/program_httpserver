#pragma once

#include "loopThread.h"

#include <vector>

class EventLoop;

// 对所有的loopthread进行分配，管理
class LoopThreadPool
{
private:
    int _thread_count;                 // 从属线程数量为0分配主线程 n RR轮询
    int _next;                         // 下一个分配的线程
    EventLoop *_base_loop;             // 主线程的EventLoop
    std::vector<EventLoop *> _loops;   // 从属线程>0则在loops中进行轮询分配
    std::vector<LoopThread *> _thread; // 保存loopthread对象
public:
    LoopThreadPool(EventLoop *baseloop);
    ~LoopThreadPool();
    void SetThreadCount(int nums)
    {
        _thread_count = nums;
    }
    void Create();
    void stop();
    EventLoop *NextLoop();
};
