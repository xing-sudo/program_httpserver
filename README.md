# program_httpserver

基于 C++20、Linux epoll 和 one-loop-per-thread Reactor 模型实现的 HTTP/1.1 服务器。

## 已实现功能

- 主从 Reactor：主 EventLoop 负责连接接入，从属 EventLoop 独立处理 I/O；
- 基于 `eventfd` 的跨线程任务投递和唤醒；
- `Buffer`、`Socket`、`Channel`、`Poller`、`EventLoop`、`Connection` 等网络组件；
- 输入输出缓冲区，支持 TCP 粘包和半包处理；
- HTTP/1.1 增量解析、大小写不敏感的请求头、Keep-Alive、HEAD、请求体大小限制和 Transfer-Encoding 校验；
- HTTP 路由、静态资源处理和目录穿越校验；
- 基于 `timerfd` 的 60 槽环形时间轮，用于连接非活跃超时管理；
- CMake 分层构建，`base`、`net`、`http` 模块分别组织头文件和源文件；
- 使用 `shared_ptr`/`weak_ptr` 保护跨线程任务和定时任务生命周期。

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

## 构建与运行

```bash
cmake -S . -B build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS=-DLOG_LEVEL=99
cmake --build build-release --target http_server -j2

# 8080 端口，2 个从属 I/O worker
taskset -c 0-1 ./build-release/http_server 8080 2
```

运行参数为 `./http_server [port] [thread_count]`。`thread_count` 表示从属 I/O worker 数量；主线程始终保留给 Acceptor 和基础 EventLoop。

## 自动化测试

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j
cmake --build build --target timer_wheel_test http_request_test http_server
ctest --test-dir build --output-on-failure
```

## 压测方法

单机压测时，将服务端绑定到 CPU 0、1，将 wrk2 绑定到 CPU 2、3。服务端需要先启动并记录 PID：

```bash
taskset -c 0-1 ./build-release/http_server 8080 1 > server-final.log 2>&1 &
echo $! > server-final.pid

# 默认测试 30000、40000、50000、55000、60000 目标 RPS
bash benchmark/local-final-bench.sh

# 也可以指定测试档位
RATES="34000 36000 38000" bash benchmark/local-final-bench.sh
```

脚本每轮同时保存 wrk2 输出、wrk2 的 `pidstat`/`mpstat` 和服务端的 `pidstat`/`mpstat`。双机测试时，客户端使用独立机器，避免本机客户端 CPU 成为瓶颈：

```bash
# 服务端
taskset -c 0-1 ./build-release/http_server 8080 1

# 客户端
wrk2 -t16 -c1000 -d30s -R2000 --latency \
  -H 'Connection: keep-alive' \
  http://服务器IP:8080/hello
```

## 性能测试概要

测试条件：Release 构建、HTTP Keep-Alive、`/hello`、1000 个并发连接、每轮 30 秒。QPS 使用 wrk2 的 `Requests/sec`，目标 RPS 和实际 QPS 分开记录。

### 单 I/O worker 本机回环

服务端绑定 CPU 0、1，wrk2 使用 CPU 2、3；`thread_count=1`。细分日志见 [`benchmark/results/local-1worker-20261023`](benchmark/results/local-1worker-20261023)。

| 目标 RPS | 实际 QPS | 平均延迟 | P99 | 服务端进程 CPU |
|---:|---:|---:|---:|---:|
| 34000 | 32364 | 5.20ms | 36.77ms | 75.7% |
| 36000 | 34237 | 9.80ms | 47.65ms | 84.2% |
| 38000 | 36160 | 25.16ms | 110.46ms | 92.2% |

单 worker 的延迟拐点约为 36000 目标 RPS；达到 38000 后尾延迟明显恶化。在 P99 小于 50ms 的标准下，36000 是当前测试的最高可接受档位。

### 双 I/O worker 本机回环

服务端使用 `thread_count=2`，客户端仍绑定两个 CPU 核。完整日志见 [`benchmark/results/local-2worker-20261023`](benchmark/results/local-2worker-20261023)。

| 目标 RPS | 实际 QPS | 平均延迟 | P99 | wrk2 CPU | 服务端进程 CPU |
|---:|---:|---:|---:|---:|---:|
| 40000 | 38074 | 3.52ms | 11.67ms | 149% | 96% |
| 50000 | 47589 | 4.48ms | 12.29ms | 172% | 119% |
| 55000 | 52337 | 5.33ms | 18.35ms | 177% | 135% |
| 60000 | 57097 | 6.59ms | 29.63ms | 182% | 150% |

在当前两核 wrk2 客户端下，55000 目标 RPS 时实际约 52300 QPS、P99 18.35ms。60000 档位中 wrk2 已接近两个客户端 CPU 核满载，因此 55000 是客户端受限环境下的观测值，不能视为服务端在更强客户端上的最终上限。

### 双机公网测试

测试服务器为 4 vCPU、4GB 云服务器，公网出口约 3Mbps；客户端使用独立机器，HTTP Keep-Alive，1000 个并发连接。

| 目标 RPS | 实际 QPS | P99 | P99.9 | 超时 |
|---:|---:|---:|---:|---:|
| 1900 | 1909 | 27.55ms | 54.24ms | 0 |
| 2000 | 1984 | 12.41ms | 20.66ms | 0 |
| 2100 | 2080 | 12.76ms | 29.58ms | 0 |
| 2200 | 1938 | 12.07s | 14.64s | 2216 |

当前公网部署条件下的稳定吞吐约为 1.9k～2.1k QPS。超过约 2.1k 后，公网出口排队导致尾延迟和超时快速增加；该数值是部署环境下的稳定吞吐，不是服务器代码在更高带宽环境中的绝对上限。

## 项目状态

当前版本已完成网络分层、HTTP 协议回归修复、时间轮生命周期处理、跨线程任务生命周期保护和基础压测记录，项目暂时冻结。后续扩展异步推理网关时，将继续完善异步响应、背压、取消和超时传播。
