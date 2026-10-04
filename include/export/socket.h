#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include<fcntl.h>
#include<errno.h>
#include"logger.h"
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
         ReuseAddr();
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