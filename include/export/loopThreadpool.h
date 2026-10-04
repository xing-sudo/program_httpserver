#pragma once

#include"eventLoop.h"
#include"loopThread.h"
#include<vector>



// 对所有的loopthread进行分配，管理
class LoopThreadPool
{
    private:
    int _thread_count;//从属线程数量为0分配主线程 n RR轮询
    int _next;//下一个分配的线程
    EventLoop* _base_loop;//主线程的EventLoop
    std::vector<EventLoop*> _loops;//从属线程>0则在loops中进行轮询分配
    std::vector<LoopThread*> _thread;//保存loopthread对象
    public:
    LoopThreadPool(EventLoop* baseloop):_base_loop(baseloop),_thread_count(0),
    _next(0){}
    ~LoopThreadPool()
    {
        for(auto &e:_thread)
        {
            delete e;
            e=nullptr;
        }
    }
    void SetThreadCount(int  nums)
    {
        _thread_count=nums;
    }
    void Create()
    {
        if(_thread_count>0)//从属线程数量必须大于0
        {
            //更新
            _thread.resize(_thread_count);
            _loops.resize(_thread_count);
            for(int i=0;i<_thread_count;i++)
            {
                _thread[i]=new LoopThread();//创建线程对象，在入口函数对EventLoop进行初始化保证线程安全
                _loops[i]=_thread[i]->GetLoop();
            }
        }
        return;
    }
    EventLoop* NextLoop()
    {
        if(_thread_count==0)
        {
            return _base_loop;
        }
        _next=(_next+1)%_thread_count;
        return _loops[_next];
    }
};