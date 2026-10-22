# 压测记录

所有数据均由 `bash benchmark/bench.sh <标签>` 生成。服务端与压测端绑核隔离，
构建统一使用 `-O2 -DLOG_LEVEL=99`（彻底关闭日志，避免 `localtime()`/`fprintf` 干扰）。

指标说明：
- **QPS** — 吞吐，`Requests/sec`
- **P50 / P99 / P99.9** — 延迟分位。wrk2 用 HdrHistogram 做了 coordinated omission 修正，
  比 wrk 的延迟分位可信
- **服务端CPU** — 压测期间服务端进程树占整机的百分比。**没打满说明瓶颈不在服务端**

## 历史数据（测量方法不规范，仅作参考）

| 标签 | 场景 | 并发 | 目标RPS | QPS | P50 | P99 | P99.9 | 服务端CPU | 备注 |
|---|---|---|---|---|---|---|---|---|---|
| pre-refactor | keep-alive | ? | wrk 尽力 | ~8900 | — | ~99ms | — | — | 单体版；未绑核、未关调频、wrk(非wrk2)、日志开启，且客户端与服务端抢同样的 4 核 |

> 注意：这一行**不能作为基线**。测量条件与下面的规范化数据不同，数字不可直接比较。
> 重构已逐函数验证为语义等价（仅 6 处行为修复，均不影响吞吐），因此规范化重测的 B0
> 数值应当与它接近——这正好可以佐证「重构未引入性能回归」。

<!-- bench.sh 会从这里往下追加 -->

## smoke-2c  (2026-10-21 18:26)

- commit: `3726588`  IO线程: 0  机器: 4核
- 服务端绑核 `0-1` / 压测端绑核 `2-3`，构建 `-O2 -DLOG_LEVEL=99`

| 标签 | 场景 | 并发 | 目标RPS | QPS | P50 | P99 | P99.9 | 服务端CPU |
|---|---|---|---|---|---|---|---|---|
| smoke-2c | keep-alive | 50 | 100 | 100.17 | 0.87ms | 2.09ms | 4.16ms | 0% |
| smoke-2c | keep-alive | 50 | 500 | 499.12 | 0.92ms | 1.97ms | 3.23ms | 1% |
| smoke-2c | keep-alive | 50 | 1000 | 995.94 | 0.89ms | 1.97ms | 2.87ms | 1% |
| smoke-2c | close | 50 | 100 | 100.18 | 0.87ms | 1.87ms | 2.12ms | 0% |
| smoke-2c | close | 50 | 500 | 499.13 | 0.91ms | 2.03ms | 2.78ms | 2% |

## baseline-2c  (2026-10-21 18:29)

- commit: `3726588`  IO线程: 0  机器: 4核
- 服务端绑核 `0-1` / 压测端绑核 `2-3`，构建 `-O2 -DLOG_LEVEL=99`

| 标签 | 场景 | 并发 | 目标RPS | QPS | P50 | P99 | P99.9 | 服务端CPU |
|---|---|---|---|---|---|---|---|---|
| baseline-2c | keep-alive | 500 | 500 | 490.29 | 1.12ms | 3.15ms | 3.61ms | 1% |
| baseline-2c | keep-alive | 500 | 1000 | 976.65 | 1.13ms | 4.22ms | 11.18ms | 1% |
| baseline-2c | keep-alive | 500 | 2000 | 1950.01 | 1.16ms | 3.37ms | 4.14ms | 2% |
| baseline-2c | keep-alive | 500 | 4000 | 3864.65 | 1.81ms | 4.80ms | 6.88ms | 4% |
| baseline-2c | keep-alive | 500 | 6000 | 5796.50 | 1.93ms | 8.17ms | 18.19ms | 5% |
| baseline-2c | keep-alive | 500 | 8000 | 7728.12 | 1.99ms | 6.56ms | 14.89ms | 7% |
| baseline-2c | keep-alive | 500 | 10000 | 9659.47 | 1.71ms | 4.14ms | 5.82ms | 7% |
| baseline-2c | close | 500 | 250 | 241.97 | 3.40ms | 9.44ms | 10.38ms | 1% |
| baseline-2c | close | 500 | 500 | 483.31 | 14.91ms | 46.01ms | 48.10ms | 2% |
| baseline-2c | close | 500 | 1000 | 976.55 | 2.39ms | 41.76ms | 44.74ms | 7% |
| baseline-2c | close | 500 | 2000 | 1932.05 | 82.82ms | 154.75ms | 201.34ms | 17% |

## baseline-2c  (2026-10-22 18:20)

- commit: `60eeb02`  IO线程: 0  机器: 4核
- 服务端绑核 `0-1` / 压测端绑核 `2-3`，构建 `-O2 -DLOG_LEVEL=99`

| 标签 | 场景 | 并发 | 目标RPS | QPS | P50 | P99 | P99.9 | 服务端CPU |
|---|---|---|---|---|---|---|---|---|
| baseline-2c | keep-alive | 500 | 500 | 483.42 | 1.60ms | 4.30ms | 5.09ms | 0% |
| baseline-2c | keep-alive | 500 | 1000 | 966.61 | 1.58ms | 3.99ms | 4.80ms | 1% |
| baseline-2c | keep-alive | 500 | 2000 | 1950.11 | 1.19ms | 3.95ms | 6.30ms | 2% |
| baseline-2c | keep-alive | 500 | 4000 | 3894.88 | 1.31ms | 3.96ms | 5.94ms | 3% |
| baseline-2c | keep-alive | 500 | 6000 | 5796.50 | 1.64ms | 5.27ms | 10.07ms | 4% |
| baseline-2c | keep-alive | 500 | 8000 | 7728.20 | 1.65ms | 4.74ms | 16.00ms | 6% |
| baseline-2c | keep-alive | 500 | 10000 | 9659.21 | 1.46ms | 4.06ms | 7.86ms | 6% |
| baseline-2c | close | 500 | 250 | 242.11 | 2.80ms | 8.97ms | 9.82ms | 0% |
| baseline-2c | close | 500 | 500 | 490.03 | 1.28ms | 9.06ms | 10.30ms | 1% |
| baseline-2c | close | 500 | 1000 | 966.98 | 2.80ms | 8.44ms | 10.54ms | 2% |
| baseline-2c | close | 500 | 2000 | 1933.36 | 2.73ms | 7.52ms | 16.43ms | 3% |
