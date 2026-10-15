#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

class EventLoop;

// 保证thread和Eventloop的对应关系，防止线程创建了后而Eventloop还没有创建，导致线程无法获取_loop
class LoopThread // 保证线程获取_loop的一致性，防止线程获取的EventLoop是未初始的值
{
private:
    EventLoop *_loop;     // eventloop必须在入口函数中实例化完成赋值不然线程不安全
    std::mutex _mutex;
    std::condition_variable _cond;
    std::thread _thread;

public:
    LoopThread(); // thread支持函数指针和参数绑定不需要bind（），用也没事
    ~LoopThread();
    void ThreadEntry();
    EventLoop *GetLoop();
    void stop();
};
