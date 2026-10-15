#include "socket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "logger.h"

#define MAX_LISTEN 1024

bool Socket::Create()
{
    _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (_sockfd < 0)
    {
        ERR_LOG("create socket failed!!");
        return false;
    }
    return true;
}

bool Socket::Bind(const std::string &ip, uint16_t port)
{
    struct sockaddr_in addr;
    struct in_addr ipv4;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if(inet_pton(AF_INET,ip.c_str(),&ipv4))
    {
        addr.sin_addr = ipv4;
    }else{
        ERR_LOG("bind ip:%s fail!",ip.c_str());
        return false;
    }
    socklen_t len = sizeof(addr);
    int ret = bind(_sockfd, (struct sockaddr *)&addr, len);
    if (ret < 0)
    {
        ERR_LOG("bind,failed!!");
        return false;
    }
    return true;
}

bool Socket::Listen()
{
    int ret = listen(_sockfd, MAX_LISTEN);
    if (ret < 0)
    {
        ERR_LOG("listen failed!!");
        return false;
    }
    return true;
}

bool Socket::Connect(const std::string &ip, uint16_t port)
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

int Socket::Accept()
{
    int newfd = accept(_sockfd, nullptr, nullptr);
    if (newfd < 0)
    {
        ERR_LOG("accept failed!!");
        return -1;
    }
    return newfd;
}

ssize_t Socket::Recv(void *buff, size_t len, int flag)
{
    ssize_t ret = recv(_sockfd, buff, len, flag);
    if (ret < 0)
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
    if(ret==0)
    {
        DBG_LOG("The other end close connection ");
        return -1;
    }
    return ret;
}

ssize_t Socket::NonBlockRecv(void *buff, size_t len)
{
    return Recv(buff, len, MSG_DONTWAIT);
    // MSG_DONTWAIT标志表示非阻塞接收，如果没有数据可读，recv函数会立即返回而不是阻塞等待
}

ssize_t Socket::Send(const void *buff, size_t len, int flag)
{
    ssize_t ret = send(_sockfd, buff, len, flag);
    if (ret == 0)
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

ssize_t Socket::NonBlockSend(const void *buff, size_t len)
{
    return Send(buff, len, MSG_DONTWAIT|MSG_NOSIGNAL);
}

void Socket::Close()
{
    if (_sockfd != -1)
    {
        close(_sockfd);
        _sockfd = -1;
    }
}

void Socket::NonBlock()
{
    int flag = fcntl(_sockfd, F_GETFL, 0);
    fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
}

void Socket::ReuseAddr()
{
    int val = 1;
    setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int));//允许断线重连使用同一个ip
    val = 1;
    setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(int));//允许多个套接字绑定同一个端口
}

// 创建服务端连接（创建，地址复用，绑定，监听，非阻塞）
bool Socket::CreateServer(uint64_t port, const std::string &ip, bool isBlock)
{
    if (Create() == false)
    {
        return false;
    }
    ReuseAddr(); // 必须在 Bind 之前设置地址复用
    if (Bind(ip, port) == false)
    {
        return false;
    }
    if (Listen() == false)
    {
        return false;
    }
    if (!isBlock)
        NonBlock();
    return true;
}

// 创建客户端连接（创建，连接）
bool Socket::CreateClient(const std::string &ip, uint64_t port, bool isBlock)
{
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
