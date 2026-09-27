#include <iostream>
#include <cstdint>
#include <vector>
#include <assert.h>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <functional>
#include <sys/epoll.h>
#include <unordered_map>
#include <memory>
#include <sys/timerfd.h>
#include <thread>
#include <mutex>
#include <sys/eventfd.h>
#include <condition_variable>
#include <typeinfo>
#include <signal.h>
// 宏函数定义
#define INF 0
#define DBG 1
#define ERR 2
#define LOG_LEVEL DBG

// strftime函数
// fprintf函数
#define LOG(level, format, ...)                                                                                       \
    do                                                                                                                \
    {                                                                                                                 \
        if (level < LOG_LEVEL)                                                                                        \
            break;                                                                                                    \
        time_t t = time(nullptr);                                                                                     \
        struct tm *ltm = localtime(&t);                                                                               \
        char tmp[32] = {0};                                                                                           \
        strftime(tmp, 31, "%H:%M:%S", ltm);                                                                           \
        fprintf(stdout, "[%p %s %s:%d]" format "\n", (void *)pthread_self(), tmp, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define INF_LOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DBG_LOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ERR_LOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)

#define BUFFER_SIZE 1024


class Buffer
{
private:
    uint64_t _write_pos;     // 写位置
    uint64_t _read_pos;      // 读位置
    std::vector<char> _data; // 容器

public:
    Buffer() : _write_pos(0), _read_pos(0), _data(BUFFER_SIZE) {}
    Buffer &operator=(const Buffer &other) // 赋值重载
    {
        if (this != &other)
        {
            _write_pos = other._write_pos;
            _read_pos = other._read_pos;
            _data = other._data; // vector的赋值操作会自动处理内存
        }
        return *this;
        // 实际上这里的赋值操作应该是深拷贝
        //_data=new vector<char>(*other._data); // 通过复制构造函数创建一个新的vector对象
    }
    char *Head() // 获取容器头位置
    {
        return &*_data.begin(); // data.begin()返回一个指向vector第一个元素的迭代器，&*操作符将其转换为指针
    }
    char *GetWritePos()
    {
        return Head() + _write_pos; // 写位置指针
    }
    char *GetReadPos()
    {
        return Head() + _read_pos; // 读位置指针
    }
    uint64_t TailSpace()
    {
        return _data.size() - _write_pos; // 尾部剩余空间
    }
    uint64_t HeadSpace()
    {
        return _read_pos; // 头部剩余空间
    }
    uint64_t ReadAbleSize()
    {
        return _write_pos - _read_pos; // 可读数据大小
    }
    // 移动读位置
    void MoveReadPos(uint64_t len)
    {
        assert(len <= ReadAbleSize());
        _read_pos += len;
    }
    // 移动写位置
    void MoveWritePos(uint64_t len)
    {
        assert(len <= TailSpace());
        _write_pos += len;
    }
    // 确保有足够的空间写入数据
    void EnsureSpace(uint64_t len)
    {
        if (TailSpace() >= len) // 如果尾部空间足够，直接返回
        {
            return;
        }
        // 如果头部和尾部空间之和足够，将可读数据移动到头部
        // copy()
        if (HeadSpace() + TailSpace() >= len)
        {
            uint64_t readable_size = ReadAbleSize();                       // 保留可读数据大小
            std::copy(GetReadPos(), GetReadPos() + readable_size, Head()); // 将可读数据移动到头部
            _read_pos = 0;                                                 // 更新读位置
            _write_pos = readable_size;                                    // 更新写位置
        }
        else
        {
            _data.resize(_write_pos + len); // 否则，扩展容器大小
        }
    }
    void Write(const void *data, uint64_t len)
    {
        if (len == 0)
        {
            return;
        }

        EnsureSpace(len);                                                       // 确保有足够空间写入数据
        std::copy((const char *)data, (const char *)data + len, GetWritePos()); // 将数据写入写位置
        MoveWritePos(len);                                                      // 更新写位置
    }
    void Write(Buffer & buf)
    {
        return Write(buf.GetReadPos(),buf.ReadAbleSize());
    }
    // 写入字符串
    void Write(const std::string &str)
    {
        Write(str.c_str(), str.size());
    }
    // 读取数据
    void Read(void *buff, uint64_t len)
    {
        assert(len <= ReadAbleSize());
        std::copy(GetReadPos(), GetReadPos() + len, (char *)buff);
        MoveReadPos(len);
    }
    // 读取指定长度的数据并返回字符串
    std::string ReadAsstring(uint64_t len)
    {
        assert(len <= ReadAbleSize());
        std::string str;
        str.resize(len);
        Read(&str[0], len);
        return str;
    }
    // 查找CRLF位置
    // memchr()
    char *FindCRLF()
    {
        char *pos = (char *)memchr(GetReadPos(), '\n', ReadAbleSize()); // 查找换行符
        return pos;
    }
    // 读取一行数据，直到CRLF
    std::string Getline()
    {
        char *pos = FindCRLF();
        if (pos == nullptr)
        {
            return "";
        }
        return ReadAsstring(pos - GetReadPos() + 1);
    }
    // 清空缓冲区
    void clear()
    {
        _write_pos = 0;
        _read_pos = 0;
    }
};

#define MAX_LISTEN 1024
// sockaddr_in,sockaddr
// setsockopt
// EAGAIN,EINTR
// fctnl
class Socket
{
private:
    int _sockfd; // 套接字文件描述符
public:
    Socket() : _sockfd(-1) {}
    Socket(int sockfd) : _sockfd(sockfd) {}
    ~Socket()
    {
        Close();
    }
    int GetFd() const
    {
        return _sockfd;
    }
    // 创建套接字
    bool Create()
    {
        _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (_sockfd < 0)
        {
            ERR_LOG("create socket failed!!");
            return false;
        }
        return true;
    }
    // 绑定地址和端口
    bool Bind(const std::string &ip, uint16_t port)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(addr);
        int ret = bind(_sockfd, (struct sockaddr *)&addr, len);
        if (ret < 0)
        {
            ERR_LOG("bind,failed!!");
            return false;
        }

        return true;
    }
    // 开始监听
    bool Listen()
    {
        int ret = listen(_sockfd, MAX_LISTEN);
        if (ret < 0)
        {
            ERR_LOG("listen failed!!");
            return false;
        }
        return true;
    }
    // 向服务器发起连接
    bool Connect(const std::string &ip, uint16_t port)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(addr);
        int ret = connect(_sockfd, (struct sockaddr *)&addr, len);
        if (ret < 0)
        {
            ERR_LOG("connect failed!!");
            return false;
        }
        return true;
    }
    // 获取新连接
    int Accept()
    {
        int newfd = accept(_sockfd, nullptr, nullptr);
        if (newfd < 0)
        {
            ERR_LOG("accept failed!!");
            return -1;
        }
        return newfd;
    }
    // 接收数据
    ssize_t Recv(void *buff, size_t len, int flag = 0)
    {
        ssize_t ret = recv(_sockfd, buff, len, flag);
        if (ret <= 0)
        {
            // EAGAIN表示非阻塞套接字没有数据可读
            // EINTR表示系统调用被信号中断，这两种情况都不是错误，可以继续尝试读取数据
            if (errno == EAGAIN || errno == EINTR)
            {
                return 0;
            }
            ERR_LOG("recv failed!!");
            return -1;
        }
        return ret;
    }
    ssize_t NonBlockRecv(void *buff, size_t len)
    {
        return Recv(buff, len, MSG_DONTWAIT);
        // MSG_DONTWAIT标志表示非阻塞接收，如果没有数据可读，recv函数会立即返回而不是阻塞等待
    }
    // 发送数据
    ssize_t Send(const void *buff, size_t len, int flag = 0)
    {
        ssize_t ret = send(_sockfd, buff, len, flag);
        if(ret==0)
        {
            // 对端关闭连接
            DBG_LOG("对端连接关闭");
            return 0;
        }
        if (ret < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
            {
                return 0;
            }
            ERR_LOG("send failed!!");
            return -1;
        }
        return ret; // 返回实际发送的字节数
    }
    ssize_t NonBlockSend(const void *buff, size_t len)
    {
        return Send(buff, len, MSG_DONTWAIT);
    }
    // 关闭套接字
    void Close()
    {
        if (_sockfd != -1)
        {
            close(_sockfd);
            _sockfd = -1;
        }
    }
    // 设置套接字为非阻塞模式
    void NonBlock()
    {
        int flag = fcntl(_sockfd, F_GETFL, 0);
        fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
    }
    // 设置套接字地址复用
    void ReuseAddr()
    {
        int val = 1;
        // setsockopt函数用于设置套接字选项\
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int));
        val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(int));
    }
    // 创建服务端连接
    bool CreateServer(uint64_t port, const std::string &ip = "0.0.0.0", bool block_flag = false)
    {
        // 创建，绑定，监听，非阻塞，地址复用
        if (Create() == false)
        {
            return false;
        }
        if (Bind(ip, port) == false)
        {
            return false;
        }
        if (Listen() == false)
        {
            return false;
        }
        if (block_flag)
            NonBlock();
        ReuseAddr();
        return true;
    }
    // 创建客户端连接
    bool CreateClient(const std::string &ip, uint64_t port, bool block_flag = false)
    {
        // 创建，连接
        if (Create() == false)
        {
            return false;
        }
        if (Connect(ip, port) == false)
        {
            return false;
        }
        return true;
    }
};
class Poller;
class EventLoop;
// 对描述符需要监控的事件和触发的事件进行管理(可读，可写,关闭，错误，任意)
class Channel
{
    /*
    EPOLLIN：表示对应的文件描述符可以读（包括对端SOCKET正常关闭）；
    EPOLLOUT：表示对应的文件描述符可以写；
    EPOLLRDHUP：表示对应的文件描述符读关闭；
    EPOLLPRI：表示对应的文件描述符有紧急数据可读；
    EPOLLHUP：表示对应的文件描述符被挂断；
    EPOLLERR：表示对应的文件描述符发生错误；
    */
private:
    int _fd; // 监控的文件描述符
    std::weak_ptr<void> tie; // 监控对象的弱引用，防止监控对象被销毁后Channel还在访问它
    bool _istie=false; // 标记是否关闭
    EventLoop *_loop;
    uint32_t _events;  // 监控的事件类型
    uint32_t _revents; // 触发的事件类型
    using Eventcallback = std::function<void()>;
    Eventcallback _read_callback;  // 可读事件回调函数
    Eventcallback _write_callback; // 可写事件回调函数
    Eventcallback _close_callback; // 关闭事件回调函数
    Eventcallback _error_callback; // 错误事件回调函数
    Eventcallback _event_callback; // 任意事件回调函数
public:
    Channel(EventLoop* loop,int fd):
    _fd(fd),
    _events(0),
    _revents(0),
    _loop(loop)
    {}
    void Tie(const std::shared_ptr<void> &obj)
    {
        tie = obj;
        _istie=true;
    } // 绑定监控对象
    int GetEvents() const
    {
        return _events;
    } // 获取监控的事件类型
    int GetFd() const
    {
        return _fd;
    } // 获取监控的文件描述符
    void SetRevents(uint32_t revents)
    {
        _revents = revents;
    } // 设置触发的事件类型
    void SetReadCallBack(const Eventcallback &cb)
    {
        _read_callback = cb;
    } // 设置可读事件回调函数
    void SetWriteCallBack(const Eventcallback &cb)
    {
        _write_callback = cb;
    } // 设置可写事件回调函数
    void SetCloseCallBack(const Eventcallback &cb)
    {
        _close_callback = cb;
    } // 设置关闭事件回调函数
    void SetErrorCallBack(const Eventcallback &cb)
    {
        _error_callback = cb;
    } // 设置错误事件回调函数
    void SetEventCallBack(const Eventcallback &cb)
    {
        _event_callback = cb;
    } // 设置任意事件回调函数
    void EnableRead()
    {
        _events |= EPOLLIN;
        Update();
    } // 开启可读事件监控
    void EnableWrite()
    {
        _events |= EPOLLOUT;
        Update();
    } // 开启可写事件监控
    void DisableRead() { 
        _events &= ~EPOLLIN; 
        Update();
    } // 关闭可读事件监控
    void DisableWrite()
    {
        _events &= ~EPOLLOUT;
        Update();
    } // 关闭可写事件监控
    void DisableAll()
    {
        _events = 0;
        Update();
    } // 关闭所有事件监控
    bool ReadAble()
    {
        return _events & EPOLLIN;
    } // 是否可读
    bool WriteAble()
    {
        return _events & EPOLLOUT;
    } // 是否可写
    void Remove();// 移除监控
    void Update();// 更新监控事件
    void HandleEvent()
    {
        std::shared_ptr<void> obj ;
        if(_istie)
        {
            obj = tie.lock();
            if(!obj)return;
        }
        if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
        {
            // 可读事件触发，EPOLLRDHUP表示对端关闭连接，EPOLLPRI表示有紧急数据可读
            if (_read_callback)
            {
                _read_callback();
            }
        }
        if (_revents & EPOLLOUT)
        {
            if (_write_callback)
            {
                _write_callback();
            }
        }
         if (_revents & EPOLLHUP)
        {
            if (_close_callback)
            {
                _close_callback();
            }
        }
         if (_revents & EPOLLERR)
        {
            if (_error_callback)
            {
                _error_callback();
            }
        }
        // 无论什么事件都要触发任意事件回调函数，刷新活跃度
        if (_event_callback)
        {
            _event_callback();
        }
    } // 处理触发的事件
};
#define MAX_EPOLLEVENTS 1024
// 通过EPOLL实现对描述符的封装
// epoll_ctl函数用于控制epoll实例的事件注册、修改和删除，
// epoll_create函数用于创建一个新的epoll实例，
// epoll_wait函数用于等待epoll实例中注册的事件发生，
class Poller
{
private:
    int _epfd;
    struct epoll_event _events[MAX_EPOLLEVENTS];  // 存储触发事件的数组
    std::unordered_map<int, Channel *> _channels; // 文件描述符到Channel对象的映射
private:
    void Updata(Channel *channel, int op)
    {
        int fd = channel->GetFd();
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = channel->GetEvents();
        int ret = epoll_ctl(_epfd, op, fd, &ev);
        if (ret < 0)
        {
            ERR_LOG("epoll_ctl failed!!");
        }
        return;
    }
    // 判断一个描述符是否添加了poller管理
    bool HasChannel(Channel *channel)
    {
        auto it = _channels.find(channel->GetFd());
        if (it == _channels.end())
        {
            return false;
        }
        return true;
    }

public:
    Poller()
    {
        _epfd = epoll_create(MAX_EPOLLEVENTS);
        if (_epfd < 0)
        {
            ERR_LOG("epoll_create failed!!");
            abort();
        }
    }
    void UpdataEvent(Channel *channel)
    {
        bool exist = HasChannel(channel);
        if (exist == false)
        {
            // 不存在添加
            _channels.insert(std::make_pair(channel->GetFd(), channel));
            return Updata(channel, EPOLL_CTL_ADD);
        }
        return Updata(channel, EPOLL_CTL_MOD);
    }
    // 删除事件
    void RemoveEvent(Channel *channel)
    {
        auto it = _channels.find(channel->GetFd());
        if (it != _channels.end())
        {
            _channels.erase(it);
        }
        return Updata(channel, EPOLL_CTL_DEL);
    }
    // 开始监控并返回活跃连接
    void Moniter(std::vector<Channel *> *active)
    {
        int nfds = epoll_wait(_epfd, _events, MAX_EPOLLEVENTS, -1);
        if (nfds < 0)
        {
            if (errno == EINTR)
            {
                return;
            }
            ERR_LOG("epoll_wait failed!!");
            abort();
        }
        for (int i = 0; i < nfds; i++)
        {
            auto it = _channels.find(_events[i].data.fd);
            assert(it != _channels.end());
            it->second->SetRevents(_events[i].events); // 设置就绪事件
            active->push_back(it->second);
        }
    }
};
// 时间轮定时器

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
//timerfd_create函数用于创建一个新的定时器文件描述符，
//settimerfd函数用于设置定时器的初始值和间隔时间，
//itimerspec结构体定义如下：
// struct itimerspec {
//     struct timespec it_interval; // 定时器的间隔时间
//     struct timespec it_value;    // 定时器的初始值
// };

class Eventloop;
class TimerWheel
{
private:
    using WeakTask = std::weak_ptr<Timetask>;
    using ShPtrTask = std::shared_ptr<Timetask>;
    int _tick;                                     /// 秒针走到哪释放哪
    int _capacity;                                 // 最大延迟时间
    std::vector<std::vector<ShPtrTask>> _wheel;    // 时间轮
    std::unordered_map<uint64_t, WeakTask> _timer; // 定时器id到定时器对象的映射

    EventLoop *_loop;
    int _timerfd; //
    std::unique_ptr<Channel> _timer_channel;

private:
    void RemoveTimer(uint64_t id)
    {
        auto it = _timer.find(id);
        if (it != _timer.end())
        {
            _timer.erase(it);
        }
    }
    static int Createtimerfd()
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
    int ReadTimerfd()
    {
        uint64_t times;
        int ret = read(_timerfd, &times, 8);
        if (ret < 0)
        {
            ERR_LOG("read timerfd failed!!");
            abort();
        }
        return (int)times;
    }
    void Run()
    {
        _tick = (_tick + 1) % _capacity; // 秒针走一步
        _wheel[_tick].clear();           // 释放该位置的所有定时器对象
    }
    void OnTime()
    {
        // 超时多少次执行多少次任务
        int count = ReadTimerfd();
        for (int i = 0; i < count; i++)
        {
            Run();
        }
    }
    void TimerAddInLoop(uint64_t id, uint32_t delay, const Taskfunc &cb)
    {
        ShPtrTask pt(new Timetask(id, delay, cb));
        pt->setDestroy(std::bind(&TimerWheel::RemoveTimer, this, id)); // 设置释放函数
        int pos = (_tick + delay) % _capacity;                        // 计算该定时器对象应该放到时间轮的哪个位置
        _wheel[pos].push_back(pt);                                    // 放入时间轮
        _timer[id] = WeakTask(pt);                                    // 存储弱引用
    }
    void TimerRefreshInLoop(uint64_t id)
    {
        //通过保存的weak_ptr对象构造share_ptr对象放入轮子
        auto it = _timer.find(id);
        if (it == _timer.end())
        {
            return;
        }
        ShPtrTask pt = it->second.lock();
        if(!pt)
        {
            ERR_LOG("TimerRefreshInLoop weak_ptr is nullptr");
        }
        int delay = pt->Delaytime();
        int pos = (_tick + delay) % _capacity; // 计算该定时器对象应该放到时间轮的哪个位置
        _wheel[pos].push_back(pt);             // 放入时间轮
    }
        void TimerCancelInLoop(uint64_t id)
    {
        auto it = _timer.find(id);
        if (it == _timer.end())
        {
            return;
        }
        ShPtrTask pt = it->second.lock();
        if (pt)
            pt->cancel();
    }
public:
    TimerWheel(EventLoop* loop) : _tick(0), _capacity(60), _wheel(_capacity),_loop(loop),
        _timerfd(Createtimerfd()),
        _timer_channel(new Channel(_loop, _timerfd))
        {
        _timer_channel->SetReadCallBack(std::bind(&TimerWheel::OnTime, this));
        _timer_channel->EnableRead();
        }
    void TimerAdd(uint64_t id, uint32_t delay, const Taskfunc &cb);
    void TimerRefresh(uint64_t id);
    void TimerCancel(uint64_t id);
    bool HasTimer(uint64_t id)
    {
        auto it = _timer.find(id);
        if (it == _timer.end())
        {
            return false;
        }
        return true;
    }
};
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
//处理任何类型的请求（协议）上下文
//不能使用模板因为模板在实例化时必须确定类型
class Any
{
    //采用父子继承的方式实现Any中保存父的指针
    //通过父类的虚函数实现对不同类型数据的操作
    private:
    class holder
    {
        public:
        virtual ~holder(){}//父类的析构函数必须是虚函数，否则子类对象被销毁时不会调用子类的析构函数，导致资源泄漏
        virtual holder* clone() const=0;//克隆函数用于复制对象
        virtual const std::type_info & type() const=0;//获取对象类型信息 
    };
    template<class T>
    class placeholder:public holder
    {
        public:
        T _data;
        public:
       placeholder(const T& data):_data(data){}
        virtual  holder* clone() const override
        {
            return new placeholder(_data);
        }
        virtual  const std::type_info& type() const override
        {
            return typeid(T);
        }
    };  
    private:
    holder* _content;//保存任意类型数据的指针
    public:
    Any():_content(nullptr){}
    template<class T>//模板构造
    Any( const T& val):_content(new placeholder<T>(val)){}
    //拷贝构造函数实现深拷贝
    Any(const Any& other):_content(other._content?other._content->clone():nullptr){}
    ~Any(){
        if(_content)
        {
            delete _content;
            _content=nullptr;
        }
    }
    Any &swap(Any& other)
    {
        std::swap(_content,other._content);
        return *this;
    }
    template<class T>
    T* Get()
    {
        assert(typeid(T)==_content->type());
        return &((placeholder<T>*)_content)->_data;
    }
    template<class T>
    Any& operator=(const T &val)
    {
        Any(val).swap(*this);
        return *this;
    }
    Any& operator=(const Any& other)
    {
        Any(other).swap(*this);
        return *this;
    }
};
class Connection;
//连接管理：套接字，丢包粘包，协议解析，连接上下文
//DISCONNECTED：连接关闭事件,CONNECTING：连接建立成功，待处理
//CONNECTED:连接建立成功，各种设置完成，可以通信；DISCONNECTING:连接待关闭
typedef enum {
    DISCONNECTED,//连接关闭
    CONNECTING,//连接建立成功，待处理
    CONNECTED,//连接建立成功，各种设置完成，可以通信
    DISCONNECTING//连接待关闭
}ConnStatus;
using PtrConnection=std::shared_ptr<Connection>;
class Connection:public std::enable_shared_from_this<Connection>//用智能指针指向自己防止其他地方将connection对象释放，导致内存访问错误
{
    private:
    uint64_t _connection_id;//连接id
    int _sockfd; //连接套接字
    bool _enable_inactive_release;//是否开启连接不活跃释放
    EventLoop* _loop;//连接所属的EventLoop
    ConnStatus _status;//连接状态
    Socket _socket;//连接套接字封装
    Channel _channel;//连接的事件管理
    Buffer _inbuffer;//连接的输入缓冲区
    Buffer _outbuffer;//连接的输出缓冲区
    Any _context;//连接上下文
    //四个回调由组件使用者决定
    using ConnectedCallBack=std::function<void(const PtrConnection&)>;
    using MessageCallBack=std::function<void(const PtrConnection&,Buffer*)>;
    using ClosedCallBack=std::function<void(const PtrConnection&)>;
    using AnyEventCallBack=std::function<void(const PtrConnection&)>;//任意事件回调函数，触发时会传递连接对象和上下文对象

    ConnectedCallBack _connected_callback;//连接建立成功回调函数
    MessageCallBack _message_callback;//消息到达回调函数
    ClosedCallBack _istie_callback;//连接关闭回调函数
    AnyEventCallBack _any_event_callback;//任意事件回调函数
    ClosedCallBack _server_istie_callback;//服务器关闭回调函数
    private:
    //五个channel事件回调
    void HandleRead()//读
    {
        //1.接收socket数据，放到输入缓冲区
        char buff[65535];
        ssize_t ret=_socket.NonBlockRecv(buff,sizeof(buff));
        if(ret<0)
        {
            //出错不能直接关闭连接，进一步判断buff状态
            return ShutdownInLoop();
        }
        //将数据放到我们的输入缓冲区
        _inbuffer.Write(buff,ret);
        //2.调用消息回调函数，处理输入缓冲区数据
        if(_inbuffer.ReadAbleSize()>0)
        {
            //shared_from_this()获取当前对象的shared_ptr智能指针
            _message_callback(shared_from_this(),&_inbuffer);
        }
    }
    void HandleWrite()
    {
        //outbuffer中就是要发送的数据
        ssize_t ret=_socket.NonBlockSend(_outbuffer.GetReadPos(),_outbuffer.ReadAbleSize());
        if(ret<0)
        {
            //发送错误关闭连接，但要判断buffer中是否还有数据
            if(_inbuffer.ReadAbleSize()>0)
            {
                //如果输入缓冲区还有数据，说明连接还活跃，不应该直接关闭连接，而是调用消息回调函数处理输入缓冲区中的数据
                _message_callback(shared_from_this(),&_inbuffer);;
            }
            return Release();
        }
        _outbuffer.MoveReadPos(ret);
         if(_outbuffer.ReadAbleSize()==0){
            _channel.DisableWrite();//没有数据待发送了关闭写监控
            if(_status==DISCONNECTING)
            {
                //如果连接状态是待关闭,说明之前是因为还有数据没有处理导致没有关闭连接
                return Release();
            }
         }
         return;
    }
    void HandleClose()
    {
        //连接关闭，套接字什么都不干了判断后直接关闭
        if(_inbuffer.ReadAbleSize()>0)
        {
            _message_callback(shared_from_this(),&_inbuffer);
        }
        return Release();
    }
    void HandleAnyEvent()
    {
        //触发任何事件都刷新活跃度，调用使用者设置的任意事件回调
        if(_enable_inactive_release==true){
            _loop->TimerRefresh(_connection_id);
        }
        if(_any_event_callback){
            _any_event_callback(shared_from_this());
        }
    }
    void HandleError()
    {
        return HandleClose();//直接复用close
    }
    //连接获取之后要对各种属性进行设置才可以通信，即connecting->connected
    void EstablishedInLoop()
    {
        //1.改状态
        assert(_status==CONNECTING);
        _status=CONNECTED;
        //2.启动读监控
        _channel.EnableRead();
        _channel.Tie(shared_from_this());//绑定监控对象，防止连接对象被销毁后channel还在访问它
        //3.调用用户设置的回调
        if(_connected_callback)
        {
            _connected_callback(shared_from_this());
        }
    }
    //实际的释放接口
    void ReleaseInLoop()
    {
        //1.修改连接状态
        _status=DISCONNECTED;
        //2.移除事件监控
        _channel.Remove();
        //3.关闭描述符
        _socket.Close();
        //4.如果当前连接在定时器队列中还有定时销毁任务则取消
        if(_loop->HasTimer(_connection_id))
        {
            CancelInactiveReleaseInLoop();
        }
        //5.调用关闭回调，避免先调用服务器回调导致连接释放，后续操作非法
        if(_istie_callback)
        {
            _istie_callback(shared_from_this());
        }
        //移除服务器内部管理信息
        if(_server_istie_callback)
        {
            _server_istie_callback(shared_from_this());
        }
    }
    //不是实际的发送接口只是将数据放到了缓冲区，并开启写监控
    void SendInLoop(Buffer &buf)
    {
        if(_status==DISCONNECTED)
        {
            return;
        }
        _outbuffer.Write(buf); 
        if(_channel.WriteAble()==false)
        {
            _channel.EnableWrite();
        }
    }
    void ShutdownInLoop()
    {
        //设置待关闭
        _status=DISCONNECTING;
        //如果输入缓冲区有数据就读取
        if(_inbuffer.ReadAbleSize()>0){
            if(_message_callback){
                _message_callback(shared_from_this(),&_inbuffer);
            }
        }
        //如果输出缓冲区有数据就发送
        if(_outbuffer.ReadAbleSize()>0)
        {
            if(_channel.WriteAble()==false)
            {
                _channel.EnableWrite();
            }
        }
        //所有都处理完了就关闭
        if(_outbuffer.ReadAbleSize()==0)
        {
            Release();
        }
    }

    void EnableInactiveReleaseInLoop(int sec)
    {
        //更改标志位
        _enable_inactive_release=true;
        //判断是否有定时销毁任务有就刷新
        if(_loop->HasTimer(_connection_id))
        {
            return _loop->TimerRefresh(_connection_id);
        }
        //没有则添加
        _loop->TimerAdd(_connection_id,sec,std::bind(&Connection::Release,this));
    }
    void CancelInactiveReleaseInLoop()
    {
        //更改标志位
        _enable_inactive_release=false;
        //有定时销毁就取消
        if(_loop->HasTimer(_connection_id))
        {
            _loop->TimerCancel(_connection_id);
        }
    }
    void UpgradeInLoop(const Any &context,const ConnectedCallBack &con,
                        const MessageCallBack& msg,const ClosedCallBack &closed,
                        const AnyEventCallBack & any)
    {
        _context=context;
        _connected_callback=con;
        _message_callback=msg;
        _istie_callback=closed;
        _any_event_callback=any;
    }
    public:
    Connection(EventLoop* loop,uint64_t id,int sockfd):_connection_id(id),_sockfd(sockfd),_loop(loop),
    _enable_inactive_release(false),_status(CONNECTING),_socket(_sockfd),_channel(loop,sockfd)
    {
        _channel.SetReadCallBack(std::bind(&Connection::HandleRead,this));
        _channel.SetWriteCallBack(std::bind(&Connection::HandleWrite,this));
        _channel.SetCloseCallBack(std::bind(&Connection::HandleClose,this));
        _channel.SetEventCallBack(std::bind(&Connection::HandleAnyEvent,this));
        _channel.SetErrorCallBack(std::bind(&Connection::HandleError,this));
    }
    ~Connection()
    {
        DBG_LOG("Release connection:%p",this);
    }
    int Fd()
    {
        return _sockfd;
    }
    //获取连接id
    int Id()
    {
        return _connection_id;
    }
    //是否为CONNECTED状态
    bool Connected()
    {
        return (_status==CONNECTED);
    }
    //设置上下文
    void SetContext(const Any& context)
    {
        _context=context;
    }
    //获取上下文
    Any* GetContext()
    {
        return &_context;
    }
    //设置各种回调
    void SetConnectedCallBack(const ConnectedCallBack& cb)
    {
        _connected_callback=cb;
    }
    void SetMessageCallBack(const MessageCallBack& cb)
    {
        _message_callback=cb;
    }
    void SetClosedCallBack(const ClosedCallBack& cb)
    {
        _istie_callback=cb;
    }
    void SetAnyEventCallBack(const AnyEventCallBack& cb)
    {
        _any_event_callback=cb;
    }
    void SetServerCloseCallBack(const ClosedCallBack& cb)
    {
        _server_istie_callback=cb;
    }
    //连接建立完毕，进行channel的各种回调设置，启动读监控，调用connected_callback
    void Established()
    {
        _loop->RunInLoop(std::bind(&Connection::EstablishedInLoop,this));
    }
    void Send(const char* data,size_t len)
    {
        //由于外界传入的data有可能是临时空间所以先将data的数据取出放入缓冲区进行操作
        Buffer tmp;
        tmp.Write(data,len);
        _loop->RunInLoop(std::bind(&Connection::SendInLoop,this,std::move(tmp)));
    }
    void Shutdown()
    {
        _loop->RunInLoop(std::bind(&Connection::ShutdownInLoop,this));
    }
    void Release()
    {
        _loop->RunInLoop(std::bind(&Connection::ReleaseInLoop,this));
    }
    void EnableInactiveRelease(int sec)
    {
        _loop->RunInLoop(std::bind(&Connection::EnableInactiveReleaseInLoop,this,sec));
    }
    void CancelInactiveRelease()
    {
        _loop->RunInLoop(std::bind(&Connection::CancelInactiveReleaseInLoop,this));
    }
    void Upgrade(const Any &context, const ConnectedCallBack &conn, const MessageCallBack &msg, 
                const ClosedCallBack &closed, const AnyEventCallBack &event)
        {
            //切换上下文必须在对应的EventLoop线程中立即执行防止新的事件触发后使用的还是原上下文
            _loop->AssertInLoop();
            _loop->RunInLoop(std::bind(&Connection::UpgradeInLoop,this,context,conn,msg,closed,event));
        }
};
//对监听套接字的管理
class Acceptor
{
    private:
    Socket _socket;
    EventLoop* _loop;//事件监控
    Channel _channel;//事件管理

    using AcceptorCallBack=std::function<void(int)>;
    AcceptorCallBack _accept_callback;//监听套接字有连接请求回调
    private:
    void HandleRead()
    {
        int newfd=_socket.Accept();
        if(newfd<0)
        {
            return;
        }
        if(_accept_callback){
            _accept_callback(newfd);
        }
    }
    int CreateServer(int port)
    {
        bool ret=_socket.CreateServer(port);
        assert(ret==true);
        return _socket.GetFd();
    }
    public:
    Acceptor(EventLoop* loop,int port):_loop(loop),_socket(CreateServer(port)),_channel(loop,_socket.GetFd())
    {
        //不能将启动读事件监控放到构造函数中
        //防止回调还没设置完成就有事件触发
        _channel.SetReadCallBack(std::bind(&Acceptor::HandleRead,this));
    }
    void SetAcceptorCallBack(const AcceptorCallBack& cb)
    {
        _accept_callback=cb;
    }
    void Listen()
    {
        _channel.EnableRead();
    }
};

class TcpServer//对所有模块整合使使用者能够轻松创建服务器
{
    private:
    uint64_t _next_id;
    int _port;
    int _timeout;
    bool _enable_inactive_release;
    Acceptor _acceptor;//监听
    EventLoop _baseloop;//事件监控
    LoopThreadPool _pool;//线程池
    std::unordered_map<uint64_t,PtrConnection> _connections;//连接管理
    
    using ConnectedCallBack=std::function<void(const PtrConnection&)>;
    using MessageCallBack=std::function<void(const PtrConnection&,Buffer*)>;
    using ClosedCallBack=std::function<void(const PtrConnection&)>;
    using AnyEventCallBack=std::function<void(const PtrConnection&)>;
    using Functor=std::function<void()>;
    ConnectedCallBack _connected_callback;
    MessageCallBack _message_callback;
    ClosedCallBack _istie_callback;
    AnyEventCallBack _any_event_callback;
    private:
    void RunAfterInLoop(const Functor& cb,int delay)
    {
        _next_id++;
        _baseloop.TimerAdd(_next_id,delay,cb);
    }
    void NewConnection(int fd){
        _next_id++;
        PtrConnection conn(new Connection(_pool.NextLoop(),_next_id,fd));
        conn->SetConnectedCallBack(_connected_callback);
        conn->SetMessageCallBack(_message_callback);
        conn->SetClosedCallBack(_istie_callback);
        conn->SetAnyEventCallBack(_any_event_callback);
        conn->SetServerCloseCallBack(std::bind(&TcpServer::RemoveConnection,this,std::placeholders::_1));
        if(_enable_inactive_release)
        {
            conn->EnableInactiveRelease(_timeout);
        }
        conn->Established();
        _connections.insert(std::make_pair(_next_id,conn));
    }
    void RemoveConnection(const PtrConnection& conn)
    {
        _baseloop.RunInLoop(std::bind(&TcpServer::RemoveConnectionInLoop,this,conn));
    }
    void RemoveConnectionInLoop(const PtrConnection& conn)
    {
        int id=conn->Id();
        auto it=_connections.find(id);
        if( it !=_connections.end()){
        _connections.erase(it);
        }
    }
    public:
    TcpServer(int port):
    _port(port),
    _next_id(0),
    _enable_inactive_release(false),
    _acceptor(&_baseloop,port),
    _pool(&_baseloop)
    {
        _acceptor.SetAcceptorCallBack(std::bind(&TcpServer::NewConnection,this,std::placeholders::_1));
        _acceptor.Listen();
    }
    void SetThreadCount(int num)
    {
        return _pool.SetThreadCount(num);
    }
    void SetConnectedCallBack(const ConnectedCallBack& cb)
    {
        _connected_callback=cb;
    }
    void SetMessageCallBack(const MessageCallBack& cb)
    {
        _message_callback=cb;
    }
    void SetClosedCallBack(const ClosedCallBack& cb)
    {
        _istie_callback=cb;
    }
    void SetAnyEventCallBack(const AnyEventCallBack& cb)
    {
        _any_event_callback=cb;
    }
    void EnableInactiveRelease(int sec)
    {
        _timeout=sec;
        _enable_inactive_release=true;
    }
    void RunAfter(const Functor& cb,int delay)
    {
        //添加定时任务
        _baseloop.RunInLoop(std::bind(&TcpServer::RunAfterInLoop,this,cb,delay));
    }
    void Start()
    {
        _pool.Create();
        _baseloop.Start();
    }
};
void Channel::Remove()
{
    return _loop->RemoveEvent(this);
}
void Channel::Update()
{
    return _loop->UpdataEvent(this);
}
void TimerWheel::TimerAdd(uint64_t id,uint32_t timeout,const Taskfunc &cb)
{
    return _loop->RunInLoop(std::bind(&TimerWheel::TimerAddInLoop,this,id,timeout,cb));
}
void TimerWheel::TimerCancel(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerCancelInLoop,this,id));
}
void TimerWheel::TimerRefresh(uint64_t id)
{
    return _loop->RunInLoop(std::bind(&TimerWheel::TimerRefreshInLoop,this,id));
}

class NetWork
{
    public:
    NetWork()
    {
        DBG_LOG("SIGPIPE INIT");
        signal(SIGPIPE,SIG_IGN);
    }
};
