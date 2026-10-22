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

cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build --output-on-failure

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

  curl http://服务器IP:8080/hello

  -服务端：
  taskset -c 0-1 ./build-release/http_server 8080 1


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
`TimerWheel::TimerRefreshInLoop` 的用户态采样占比从约 21.6% 降至约 1%。

第二次测试
测试条件：

- Release 构建
- 服务端 4 vCPU、3 Mbps 公网出口
- 服务端绑核 `0-1`
- 客户端 `wrk2 -t16 -c1000`
- HTTP Keep-Alive
- 请求路径 `/hello`
- 每轮测试 30 秒

| 目标 RPS | 实际 QPS | P99 | P99.9 | Timeout |
|---:|---:|---:|---:|---:|
| 1500 | 1513 | 12.24ms | 44.45ms | 0 |
| 1800 | 1803 | 144.77ms | 165.76ms | 0 |
| 1900 | 1909 | 27.55ms | 54.24ms | 0 |
| 2000 | 1984 | 12.41ms | 20.66ms | 0 |
| 2100 | 2080 | 12.76ms | 29.58ms | 0 |
| 2200 | 1938 | 12.07s | 14.64s | 2216 |

当前测试中，稳定吞吐约为 2.0～2.1k QPS。目标速率达到 2.2k QPS 后，公网出口出现排队，P99 上升到秒级并产生大量超时，服务端 CPU 在压测期间没有打满，当前吞吐边界主要受到公网出口带宽限制。该数值代表当前云服务器部署条件下的稳定吞吐，不代表服务器代码在更高带宽环境中的绝对上限。
## 当前状态以及后续目标：
双机测试命令:
服务端：
cmake -S . -B build-bench \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-DLOG_LEVEL=99" \

cmake --build build-bench --target http_server -j2 \

taskset -c 0-1 \
  ./build-bench/http_server 8080 1 \ -----
  > server-runtime.log 2>&1 &

客户端：
for rate in 1500 1800 1900 2000 2100 2200; do
    taskset -c 16-31 wrk2 \
      -t16 -c1000 -d30s -R"$rate" --latency \
      -H 'Connection: keep-alive' \
      http://服务器IP:8080/hello \
      2>&1 | tee "client-keepalive-${rate}.log"

    sleep 10
done
> 状况：
4核4G的云服务器在 3 Mbps 公网出口带宽、双机 wrk2、1000 并发连接和 HTTP Keep-Alive 条件下，服务器稳定处理约 1.9k QPS，P99 延迟约 20ms。目标速率超过约 2.1k QPS 后，公网出口出现发送排队，尾延迟和请求超时明显增加。
> 后续：
- 当前正在继续优化事件处理、连接管理和请求路径，后续计划扩展为异步推理网关。
