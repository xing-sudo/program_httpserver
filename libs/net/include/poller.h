#pragma once

#include <sys/epoll.h>

#include <unordered_map>
#include <vector>
#include<unistd.h>

class Channel;

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
    void Updata(Channel *channel, int op);
    bool HasChannel(Channel *channel); // 判断一个描述符是否添加了poller管理

public:
    Poller();
    ~Poller(){close(_epfd);}
    void UpdataEvent(Channel *channel);
    void RemoveEvent(Channel *channel);           // 删除事件
    void Moniter(std::vector<Channel *> *active); // 开始监控并返回活跃连接
};
