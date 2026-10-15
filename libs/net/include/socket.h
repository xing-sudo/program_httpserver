#pragma once

#include <sys/types.h>

#include <cstddef>
#include <cstdint>
#include <string>

// sockaddr_in,sockaddr
// setsockopt
// EAGAIN,EINTR
// fcntl
class Socket
{
private:
    int _sockfd; // 套接字文件描述符
public:
    Socket() : _sockfd(-1) {}
    Socket(int sockfd) : _sockfd(sockfd) {}
    ~Socket() { Close(); }
    int GetFd() const { return _sockfd; }

    bool Create();                                                       // 创建套接字
    bool Bind(const std::string &ip, uint16_t port);                     // 绑定地址和端口
    bool Listen();                                                       // 开始监听
    bool Connect(const std::string &ip, uint16_t port);                  // 向服务器发起连接
    int Accept();                                                        // 获取新连接
    ssize_t Recv(void *buff, size_t len, int flag = 0);                  // 接收数据
    ssize_t NonBlockRecv(void *buff, size_t len);
    ssize_t Send(const void *buff, size_t len, int flag = 0);            // 发送数据
    ssize_t NonBlockSend(const void *buff, size_t len);
    void Close();                                                        // 关闭套接字
    void NonBlock();                                                     // 设置套接字为非阻塞模式
    void ReuseAddr();                                                    // 设置套接字地址复用
    bool CreateServer(uint64_t port, const std::string &ip = "0.0.0.0", bool isBlock = false); // 创建服务端连接
    bool CreateClient(const std::string &ip, uint64_t port, bool isBlock = false);             // 创建客户端连接
};
