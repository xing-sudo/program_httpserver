#include "poller.h"

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <utility>

#include "channel.h"
#include "logger.h"

Poller::Poller()
{
    _epfd = epoll_create(MAX_EPOLLEVENTS);
    if (_epfd < 0)
    {
        ERR_LOG("epoll_create failed!!");
        abort();
    }
}

void Poller::Updata(Channel *channel, int op)
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

bool Poller::HasChannel(Channel *channel)
{
    auto it = _channels.find(channel->GetFd());
    if (it == _channels.end())
    {
        return false;
    }
    return true;
}

void Poller::UpdataEvent(Channel *channel)
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

void Poller::RemoveEvent(Channel *channel)
{
    auto it = _channels.find(channel->GetFd());
    if (it != _channels.end())
    {
        _channels.erase(it);
    }
    return Updata(channel, EPOLL_CTL_DEL);
}

void Poller::Moniter(std::vector<Channel *> *active)
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
