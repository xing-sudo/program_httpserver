#include "loopThread.h"

#include "eventLoop.h"

LoopThread::LoopThread() : _loop(nullptr), _thread(std::thread(&LoopThread::ThreadEntry, this))
{
}
LoopThread::~LoopThread()
{
    stop();
}
void LoopThread::stop()
{
    if (_loop)
    {
        _loop->stop();
        _loop = nullptr;
    }
    if (_thread.joinable())
    {

        _thread.join();
    }
}
void LoopThread::ThreadEntry()
{
    EventLoop loop;
    {
        std::unique_lock<std::mutex> _lock(_mutex);
        _loop = &loop;
        _cond.notify_all(); // 唤醒cond上阻塞的线程
    }
    loop.Start();
}

EventLoop *LoopThread::GetLoop()
{
    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> _lock(_mutex);
        _cond.wait(_lock, [&]()
                   { return _loop != nullptr; }); // loop为null就一直阻塞
        loop = _loop;
    }
    return loop;
}
