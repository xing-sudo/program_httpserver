#pragma once

#include<sys/epoll.h>
#include<unordered_map>
#include"logger.h"


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