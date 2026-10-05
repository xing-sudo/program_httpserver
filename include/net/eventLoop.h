#pragma once

#include"timeWheel.h"
#include"poller.h"
#include"channel.h"
#include"logger.h"
#include<fuctional>
#include<sys/eventfd.h>
#include<vector>
#include<mutex>
#include<thread>
#include<memory>

class EventLoop
{
    private:
    using Func=std::function<void()>;
    std::thread::id _thread_id; //线程id
    int _event_fd;
    std::unique_ptr<Channel> _event_channel;// 事件fd的Channel对象
    Poller _poller;
    std::vector<Func> _tasks;//任务池保护线程安全
    std::mutex _mutex;// 保护任务池线程安全
    TimerWheel _timer_wheel;//时间轮定时器
    public:
    void RunAllTask()
    {
        //使用交换进行执行任务
        std::vector<Func> tmp;
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            tmp.swap(_tasks);
        }
        for(auto &e:tmp)
        {
            e();
        }
        return;
    }
    //对eventfd的操作封装
    static int CreatEventFd()
    {
         int efd=eventfd(0,EFD_CLOEXEC|EFD_NONBLOCK);
            if(efd<0)
            {
                ERR_LOG("create eventfd failed!!");
               abort();
            }
            return efd;
    }
    void ReadEeventFd()
    {
        uint64_t val=0;
        ssize_t res=read(_event_fd,&val,8);
        if (res<0)
        {
            if(errno==EAGAIN||errno==EINTR)
            {
                return;
            }
            ERR_LOG("read eventfd failed!!");
            abort();
        }
        return;
    }
    void WakeUpEventFd()
    {
        uint64_t val=1;
        ssize_t res=write(_event_fd,&val,sizeof(val));
        if(res<0)
        {
            if(errno==EINTR)
            {
                return;
            }
            ERR_LOG("write eventfd failed!!");
            abort();
        }
        return;
    }
    public:
    EventLoop():_thread_id(std::this_thread::get_id()),
    _event_fd(CreatEventFd()),
    _event_channel(new Channel(this,_event_fd)),
    _timer_wheel(this)
    {
        //给eventfd设置读回调 读取通知次数 类内调用成员函数要传递this指针
        _event_channel->SetReadCallBack(std::bind(&EventLoop::ReadEeventFd,this));
        _event_channel->EnableRead();
    }
    void Start()
    {
        //1.监控2.就绪处理3.执行任务
        while(1)
        {
            //1.
            std::vector<Channel*> active;
            _poller.Moniter(&active);//接口是接口，调用者自己会对epfd进行updata
            //2.
            for(auto &e:active)
            {
                e->HandleEvent();
            }
            //3.
            RunAllTask();
        }
    }
    //判断当前线程是否是EventLoop所对应的线程
    bool IsInLoopThread()
    {
        return _thread_id==std::this_thread::get_id();
    }
    //保证当前线程就是要执行任务的线程
    void AssertInLoop()
    {
        assert(_thread_id==std::this_thread::get_id());
    }
    void RunInLoop(const Func& cb)
    {
        //判断当前线程和EventLoop所对应的线程是否相同，如果相同直接执行，否则加入任务池
        if(IsInLoopThread())
        {
            return cb();
        }
        return AddTasks(cb);
    }
    void AddTasks(const Func &cb)
    {
        {
            std::unique_lock<std::mutex> _lock (_mutex);
            _tasks.push_back(cb);
        }
        //唤醒EventLoop所在的线程，让其执行任务
        //通过eventfd进行线程间通知
        //防止因为eventfd没有就绪导致epoll阻塞
        WakeUpEventFd();
    }
    //对poller channel timerwheel的接口进行封装，供外部调用
    //添加/修改描述符的事件监控
    void UpdataEvent(Channel* channel)
    {
        _poller.UpdataEvent(channel);
    }
    //移除描述符的事件监控
    void RemoveEvent(Channel* channel)
    {
        _poller.RemoveEvent(channel);
    }
    void TimerAdd(uint64_t id, uint32_t delay, const Taskfunc &cb)
    {
        _timer_wheel.TimerAdd(id, delay, cb);
    }
    void TimerRefresh(uint64_t id)
    {
        _timer_wheel.TimerRefresh(id);
    }
    void TimerCancel(uint64_t id)
    {
        _timer_wheel.TimerCancel(id);
    }
    bool HasTimer(uint64_t id)
    {
        return _timer_wheel.HasTimer(id);
    }
};