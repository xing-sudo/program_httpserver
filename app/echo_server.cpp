// echo_main.cpp
// EchoServer 测试入口
// 编译命令：g++ -o echo_server echo_main.cpp -std=c++11 -lpthread -O2
// 运行：./echo_server
// 测试：telnet 127.0.0.1 9090  或  nc 127.0.0.1 9090

#include "source.cpp"


// 全局 SIGPIPE 屏蔽（防止对端关闭时服务器崩溃）
static NetWork g_network;

// ---- 回调函数 ----

// 连接建立成功时触发
void OnConnected(const PtrConnection& conn)
{
    INF_LOG("[EchoServer] New connection: fd=%d, id=%d", conn->Fd(), conn->Id());
}

// 有数据到达时触发：把收到的数据原样回写
void OnMessage(const PtrConnection& conn, Buffer* buf)
{
    // 读取收到的所有数据
    std::string msg = buf->ReadAsstring(buf->ReadAbleSize());
    INF_LOG("[EchoServer] Recv(%d bytes): %s", (int)msg.size(), msg.c_str());
    // 原样回写
    conn->Send(msg.c_str(), msg.size());
}

// 连接关闭时触发
void OnClosed(const PtrConnection& conn)
{
    INF_LOG("[EchoServer] Connection closed: id=%d", conn->Id());
}

int main()
{
    // 创建 TcpServer，监听 9090 端口
    TcpServer server(9090);

    // 设置线程池线程数（0 = 单线程，主线程自己处理所有 IO）
    // 多线程可以改成：server.SetThreadCount(3);
    server.SetThreadCount(0);

    // 注册回调
    server.SetConnectedCallBack(OnConnected);
    server.SetMessageCallBack(OnMessage);
    server.SetClosedCallBack(OnClosed);

    // 开启非活跃连接自动释放（30秒无事件则断开）
    server.EnableInactiveRelease(30);

    INF_LOG("[EchoServer] Listening on port 9090 ...");

    // 启动事件循环（阻塞在这里）
    server.Start();

    return 0;
}
