# program_httpserver

基于 C++20、Linux epoll 的 Reactor HTTP 服务器。

## 已实现功能

- 主从 Reactor 网络模型，主线程负责连接接入，从属 EventLoop 线程负责 I/O；
- 基于 eventfd 的跨线程任务通知；
- Buffer、Socket、Channel、Poller、EventLoop、Connection 等网络组件；
- 输入输出缓冲区，支持 TCP 粘包和半包处理；
- HTTP/1.1 增量解析、Keep-Alive、HEAD、请求体长度限制和 Transfer-Encoding 校验；
- HTTP 路由和静态资源处理；
- 60 槽环形时间轮，用于连接非活跃超时管理；
- shared_ptr/weak_ptr 管理跨线程任务和定时任务生命周期；
- CMake 分层构建，base、net、http 模块分别组织头文件和源文件。

## 目录结构

```text
base/
  include/
  src/
net/
  include/
  src/
http/
  include/
  src/
app/
tests/
benchmark/
```

## 构建方式
cmake -S . -B build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-DLOG_LEVEL=99"

cmake --build build-release --target http_server -j2

## 运行与测试
- 单机：
THREADS=0 \
DURATION=30 \
CONNS=500 \
RATES="500 1000 2000 4000 6000 8000 10000" \
CLOSE_RATES="250 500 1000 2000" \
bash benchmark/bench.sh baseline-2c

- 双机：
- 客户端（尽量使用Linux环境）
  wrk2 -t16 -c1000 -d30s -R1900 --latency \
  -H 'Connection: keep-alive' \
  http://服务器IP:8080/hello

  curl 服务端ip：port

  -服务端：
  taskset -c 0-1 ./build-release/http_server 8080 1

## 性能测试
## 性能测试

测试构建：

- Release
- C++20
- 日志通过 `-DLOG_LEVEL=99` 编译关闭
- HTTP Keep-Alive
- `/hello`
- wrk2 双机压测

当前双机基线：

| 目标速率 | 并发连接 | 实际 QPS | P99 | P99.9 | 超时 |
|---:|---:|---:|---:|---:|---:|
| 1900 | 1000 | 1865 | 17.55ms | 20.82ms | 0 |
| 2000 | 1000 | 1962 | 17.93ms | 216.83ms | 0 |
| 2150 | 1000 | 1999 | 3.52s | 6.25s | 490 |
| 2200 | 1000 | 1730 | 16.55s | 20.22s | 3845 |

在当前测试环境下，服务器的稳定吞吐约为 1.9k QPS。目标速率超过约 2.1k QPS 后，尾延迟和超时明显增加。
时间轮优化前后，本机 perf 中
`TimerWheel::TimerRefreshInLoop` 的用户态采样占比从约 21.6% 降至约 1%。双机吞吐和尾延迟将在相同参数下重新验证。
## 当前状态以及后续目标：
在当前测试环境中，Keep-Alive、1000 并发连接下，服务器稳定处理约 1900 QPS，P99 延迟约 20ms。超过约 2100 QPS 后，尾延迟和请求超时明显增加。
当前正在继续优化事件处理、连接管理和请求路径，后续计划扩展为异步推理网关。
