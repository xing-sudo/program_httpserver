#include "timeWheel.h"

#include <sys/timerfd.h>
#include <unistd.h>

#include <cstdlib>
#include <functional>

#include "eventLoop.h"
#include "logger.h"

void TimerWheel::RemoveTimer(uint64_t id)
{
    detachNode(id);
}

int TimerWheel::Createtimerfd()
{
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timerfd < 0)
    {
        ERR_LOG("timerfd_create failed!!");
        abort();
    }
    struct itimerspec itime;
    itime.it_value.tv_sec = 1;
    itime.it_value.tv_nsec = 0;
    itime.it_interval.tv_sec = 1;
    itime.it_interval.tv_nsec = 0;
    timerfd_settime(timerfd, 0, &itime, nullptr);
    return timerfd;
}

int TimerWheel::ReadTimerfd()
{
    uint64_t times;
    int ret = read(_timerfd, &times, 8);
    if (ret < 0)
    {
        ERR_LOG("read timerfd failed!!");
        abort();
    }
    return static_cast<int>(times);
}
void TimerWheel::Run(int ticks)
{
   for(int i=0;i<ticks;i++)// 秒针走一步
   {
    _tick=(_tick+1)%_capacity;
    executeTask();
   }
}
void TimerWheel::executeTask()
{
    //拿到槽位的任务，随后先清空槽位再执行任务，圈数>0不执行
    auto &queue = _wheel[_tick];
    std::vector<std::shared_ptr<Timetask>> tmp;
    size_t pos = 0;
    while (pos < queue.size())
    {
        const uint64_t id = queue[pos].id;
        auto &task = queue[pos].task;
        if (task->getCount() > 0)
        {
            task->decreaseCount();
            pos++;
            continue;
        }
        tmp.emplace_back(detachNode(id));//pos不++因为换过来的任务
    }
    for (auto &task : tmp)
    {
        task->executeTask();
    }
}

void TimerWheel::OnTime()
{
    int ticks = ReadTimerfd();//读取当前已经超时了多久
    Run(ticks);//推进秒针前进
}

void TimerWheel::TimerAddInLoop(uint64_t id, uint32_t delay, const Taskfunc &cb)
{
    auto isExist = _timer.find(id);
    if (isExist != _timer.end())
    {
        detachNode(id);
    }
    TimeNode newNode;
    newNode.id = id;
    newNode.task = std::make_shared<Timetask>(id, delay, cb);
    newNode.task->setDestroy([it = weak_from_this(), id]()
                             {
    if(auto ptr=it.lock())
    {
        ptr->RemoveTimer(id);
    } });             // 设置释放函数
    size_t pos = (_tick + delay) % _capacity;  // 计算该定时器对象应该放到时间轮的哪个位置
    _wheel[pos].push_back(std::move(newNode)); // 放入时间轮

    TimePosition tp;
    tp.index = _wheel[pos].size() - 1;
    tp.slot = pos;
    _timer[id] = std::move(tp); // 存储弱引用
}
std::shared_ptr<Timetask> TimerWheel::detachNode(uint64_t id)
{
    // 用swap-pop的思路，先查哈希表定位他在轮子里的槽位，拿到槽位的vector进行操作，取出shareptr，再判断是不是最后一个元素，直接用移动转移资源
    auto pos = _timer.find(id);
    if (pos == _timer.end())
    {
        return {};
    }
    const auto [slot, index] = pos->second; // 哈希表里的second起别名
    auto &queue = _wheel[slot];
    auto task = std::move(queue[index].task);
    const size_t last = queue.size() - 1;
    if (index != last)
    {
        queue[index] = std::move(queue[last]);
        _timer[queue[index].id].index = index; // 找到原末尾元素在哈希表的index改成现在的index
    }
    queue.pop_back();
    _timer.erase(pos);

    return task;
}
void TimerWheel::TimerRefreshInLoop(uint64_t id)
{
    // 先查看需要刷新的对象是否存在以及是否正在执行，再给它删掉，计算新的位置并放入
    auto posTimer = _timer.find(id);
    if (posTimer == _timer.end())
    {
        return;
    }
    auto &oldNode = _wheel[posTimer->second.slot][posTimer->second.index];

    if (oldNode.task->isInExecute())
    {
        return;
    }
    auto task = detachNode(id);
    if (!task)
    {
        return;
    }
    const uint32_t delay = task->Delaytime();
    task->resetCount(delay);

    const uint32_t newpos = static_cast<uint32_t>((_tick + delay) % _capacity); // 计算该定时器对象应该放到时间轮的哪个位置
    auto &queue = _wheel[newpos];                                               // 放入时间轮
    const uint32_t newindex = static_cast<uint32_t>(queue.size());
    queue.emplace_back(TimeNode{id, std::move(task)});
    _timer.emplace(id, TimePosition{newpos, newindex});
}

void TimerWheel::TimerCancelInLoop(uint64_t id)
{
    auto task=detachNode(id);
    if(task)
    {
        task->cancel();
    }
}

TimerWheel::TimerWheel(EventLoop *loop) : _tick(0), _capacity(60), _wheel(_capacity), _loop(loop),
                                          _timerfd(Createtimerfd()),
                                          _timer_channel(new Channel(_loop, _timerfd))
{
    _timer_channel->SetReadCallBack(std::bind(&TimerWheel::OnTime, this));
    _timer_channel->EnableRead();
}

void TimerWheel::TimerAdd(uint64_t id, uint32_t delay, const Taskfunc &cb)
{
    std::weak_ptr<TimerWheel> it = shared_from_this();
    _loop->RunInLoop([it, id, delay, cb]()
                     {
        if(auto ptr=it.lock())
        {
            ptr->TimerAddInLoop(id,delay,cb);
        } });
}

void TimerWheel::TimerCancel(uint64_t id)
{
    std::weak_ptr<TimerWheel> it = shared_from_this();
    _loop->RunInLoop([it, id]()
                     {
        if(auto ptr=it.lock())
        {
            ptr->TimerCancelInLoop(id);
        } });
}

void TimerWheel::TimerRefresh(uint64_t id)
{
    std::weak_ptr<TimerWheel> it = shared_from_this();
    _loop->RunInLoop([it, id]()
                     {
        if(auto ptr=it.lock())
        {
            ptr->TimerRefreshInLoop(id);
        } });
}
