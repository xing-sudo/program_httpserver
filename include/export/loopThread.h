#pragma once

#include"eventLoop.h"
#include<thread>
#include<mutex>
#include<condition_variable>



// 保证thread和Eventloop的对应关系，防止线程创建了后而Eventloop还没有创建，导致线程无法获取_loop
class LoopThread//保证线程获取_loop的一致性，防止线程获取的EventLoop是未初始的值
{
    private:
    std::thread _thread;
    std::mutex _mutex;
    std::condition_variable _cond ;
    EventLoop* _loop; //eventloop必须在入口函数中实例化完成赋值不然线程不安全
    public:
    LoopThread():_loop(nullptr),_thread(std::thread(&LoopThread::ThreadEntry,this))//thread支持函数指针和参数绑定不需要bind（），用也没事
    {}
    void ThreadEntry()
    {
        EventLoop loop;
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _loop=&loop;
            _cond.notify_all();//唤醒cond上阻塞的线程
        }
        loop.Start();
    }
    EventLoop* GetLoop()
    {
        EventLoop* loop=nullptr;
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _cond.wait(_lock,[&](){ return _loop!=nullptr;});//loop为null就一直阻塞
            loop=_loop;
        }
        return loop;
    }
};