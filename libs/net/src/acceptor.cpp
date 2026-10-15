#include "acceptor.h"
void Acceptor::HandleRead()
{
    int newfd = _socket.Accept();
    if (newfd < 0)
    {
        return;
    }
    if (_accept_callback)
    {
        _accept_callback(newfd);
    }
}
int Acceptor::CreateServer(int port)
{
    bool ret = _socket.CreateServer(port);
    assert(ret == true);
    return _socket.GetFd();
}
Acceptor::Acceptor(EventLoop *loop, int port) : _loop(loop), _socket(CreateServer(port)), _channel(loop, _socket.GetFd())
{
    // 不能将启动读事件监控放到构造函数中
    // 防止回调还没设置完成就有事件触发
    _channel.SetReadCallBack(std::bind(&Acceptor::HandleRead, this));
}