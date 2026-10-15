#include "loopThreadpool.h"

LoopThreadPool::LoopThreadPool(EventLoop *baseloop) : _base_loop(baseloop), _thread_count(0),
                                                     _next(0) {}

LoopThreadPool::~LoopThreadPool()
{
    stop();
    for(auto &e:_thread)
    {
        delete e;
        e=nullptr;
    }
}

void LoopThreadPool::Create()
{
    if (_thread_count > 0) // 从属线程数量必须大于0
    {
        // 更新
        _thread.resize(_thread_count);
        _loops.resize(_thread_count);
        for (int i = 0; i < _thread_count; i++)
        {
            _thread[i] = new LoopThread(); // 创建线程对象，在入口函数对EventLoop进行初始化保证线程安全
            _loops[i] = _thread[i]->GetLoop();
        }
    }
    return;
}
void LoopThreadPool::stop()
{
    size_t threadNums=_thread.size();
    for(size_t i=0;i<threadNums;i++)
    {
        _thread[i]->stop();
    }
}
EventLoop *LoopThreadPool::NextLoop()
{
    if (_thread_count == 0)
    {
        return _base_loop;
    }
    _next = (_next + 1) % _thread_count;
    return _loops[_next];
}
