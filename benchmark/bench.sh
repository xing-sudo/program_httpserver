#!/usr/bin/env bash
#
# 基线压测脚本 —— 绑核 + 关日志构建 + wrk2 扫速率 + 自动记录
#
# 用法：
#   bash benchmark/bench.sh                 # 用当前 commit 短 hash 当标签
#   bash benchmark/bench.sh B0              # 自定义标签（基线）
#   bash benchmark/bench.sh O1-no-regex     # 某一轮优化
#
# 可调环境变量：
#   THREADS=1        SetThreadCount（0=纯单线程 Reactor，1=1主1从，默认 1）
#   DURATION=30      每轮压测秒数
#   CONNS=200        并发连接数
#   RATES="2000 5000 10000 20000 40000"   扫的速率点（wrk2 的 -R，必填）
#   CLOSE_RATES="1000 2000 5000"          短连接扫的速率点
#   PORT=8080
#
# 结果追加到 benchmark/results.md

set -uo pipefail

LABEL="${1:-$(git rev-parse --short HEAD 2>/dev/null || echo unknown)}"
PORT="${PORT:-8080}"
DURATION="${DURATION:-30}"
CONNS="${CONNS:-200}"
THREADS="${THREADS:-1}"
RATES="${RATES:-2000 5000 10000 20000 40000}"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

BUILD=build-bench
BIN=$BUILD/http_server
OUT=benchmark/results.md
RAW_DIR="${RAW_DIR:-benchmark/raw}"
SRV_LOG=$(mktemp)

mkdir -p "$RAW_DIR"

# ---------------------------------------------------------------- 核分配
# 服务端和压测客户端各占一半，避免互相抢核污染数据
TOTAL=$(nproc 2>/dev/null || echo 4)
if [ "$TOTAL" -ge 4 ]; then
    HALF=$((TOTAL / 2))
    SRV_CORES="0-$((HALF - 1))"
    CLI_CORES="$HALF-$((TOTAL - 1))"
    WRK_T=$HALF
elif [ "$TOTAL" -eq 2 ]; then
    # 两核机器上必须给客户端单独留一个核，否则服务端和 wrk2 会互相抢占。
    # 此时建议 THREADS=0，把服务端作为单线程 Reactor 进行基线测试。
    SRV_CORES="0"
    CLI_CORES="1"
    WRK_T=1
else
    SRV_CORES="0"
    CLI_CORES="0"
    WRK_T=1
fi

echo "=============================================="
echo " 标签       : $LABEL"
echo " commit     : $(git rev-parse --short HEAD 2>/dev/null)"
echo " 机器       : ${TOTAL} 核"
echo " 服务端绑核 : $SRV_CORES"
echo " 压测端绑核 : $CLI_CORES  (wrk2 -t$WRK_T)"
echo " IO 线程数  : $THREADS"
echo "=============================================="

# ---------------------------------------------------------------- 依赖检查
if ! command -v wrk2 >/dev/null 2>&1; then
    echo "❌ 未找到 wrk2。安装："
    echo "   git clone https://github.com/giltene/wrk2.git ~/wrk2 && make -C ~/wrk2 -j4 && sudo cp ~/wrk2/wrk /usr/local/bin/wrk2"
    exit 1
fi
command -v cmake >/dev/null 2>&1 || { echo "❌ 未找到 cmake：sudo apt install -y cmake build-essential"; exit 1; }
command -v g++   >/dev/null 2>&1 || { echo "❌ 未找到 g++：sudo apt install -y build-essential"; exit 1; }

# ---------------------------------------------------------------- 关 CPU 调频
GOV=/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
if [ -w "$GOV" ]; then
    for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
        echo performance | sudo tee "$g" >/dev/null 2>&1 || true
    done
    echo "governor : $(cat $GOV)"
else
    echo "⚠️  读不到 cpufreq（云主机/虚拟机常见），调频可能影响数据稳定性"
fi

# ---------------------------------------------------------------- 构建
# -DLOG_LEVEL=99 让所有日志被编译掉：LOG 宏每行都要走 localtime()+fprintf，
# 不关掉的话测的是日志而不是服务器
echo "--- 构建 ---"
cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-O2 -DLOG_LEVEL=99" >/dev/null || exit 1
cmake --build "$BUILD" --target http_server -j"$TOTAL" >/dev/null || exit 1
echo "构建完成: $BIN"

# ---------------------------------------------------------------- 起服务端
pkill -f "$BUILD/http_server" 2>/dev/null
sleep 0.5
taskset -c "$SRV_CORES" "$BIN" "$PORT" "$THREADS" >"$SRV_LOG" 2>&1 &
SRV_PID=$!
trap 'kill $SRV_PID 2>/dev/null' EXIT
sleep 1

if ! kill -0 "$SRV_PID" 2>/dev/null; then
    echo "❌ 服务端启动失败："
    cat "$SRV_LOG"
    exit 1
fi
echo "服务端就绪 pid=$SRV_PID  (日志: $SRV_LOG)"

# ---------------------------------------------------------------- CPU 采样
CLK=$(getconf CLK_TCK)
proc_ticks() {
    local sum=0 f
    for f in /proc/"$SRV_PID"/task/*/stat; do
        [ -r "$f" ] || continue
        sum=$((sum + $(awk '{print $14+$15}' "$f")))
    done
    echo "$sum"
}

# ---------------------------------------------------------------- 跑一轮
# $1=场景描述 $2=目标速率（wrk2 的 -R 是必填项，不能省）其余参数透传给 wrk2
run_one() {
    local desc="$1" rate="$2"; shift 2
    local out rps p50 p99 p999 util t0 t1 raw_file safe_label safe_desc

    safe_label=$(printf '%s' "$LABEL" | tr -c 'A-Za-z0-9_.-' '_')
    safe_desc=$(printf '%s' "$desc" | tr -c 'A-Za-z0-9_.-' '_')
    raw_file="$RAW_DIR/${safe_label}_${safe_desc}_${rate}.log"

    t0=$(proc_ticks)
    if ! out=$(taskset -c "$CLI_CORES" wrk2 -t"$WRK_T" -c"$CONNS" -d"$DURATION" -R"$rate" --latency "$@" \
          "http://127.0.0.1:$PORT/hello" 2>&1 | tee "$raw_file"); then
        echo "❌ wrk2 failed in $desc at RPS $rate; raw output: $raw_file" >&2
        return 1
    fi
    t1=$(proc_ticks)

    rps=$(awk '/^Requests\/sec:/{print $2}' <<<"$out")
    p50=$(awk '/^ *50\.000%/{print $2}' <<<"$out")
    p99=$(awk '/^ *99\.000%/{print $2}' <<<"$out")
    p999=$(awk '/^ *99\.900%/{print $2}' <<<"$out")
    util=$(awk -v a="$t0" -v b="$t1" -v d="$DURATION" -v hz="$CLK" -v n="$TOTAL" \
           'BEGIN{printf "%.0f%%", (b-a)/hz/d/n*100}')

    printf '%-28s RPS=%-10s QPS=%-10s P50=%-10s P99=%-10s P99.9=%-10s CPU=%s\n' \
           "$desc" "${rate:--}" "${rps:-ERR}" "${p50:-?}" "${p99:-?}" "${p999:-?}" "$util"
    echo "  raw output: $raw_file"

    echo "| $LABEL | $desc | $CONNS | ${rate:--} | ${rps:-ERR} | ${p50:-?} | ${p99:-?} | ${p999:-?} | $util |" >>"$OUT"
}

# ---------------------------------------------------------------- 记录表头
{
    echo ""
    echo "## $LABEL  ($(date '+%Y-%m-%d %H:%M'))"
    echo ""
    echo "- commit: \`$(git rev-parse --short HEAD 2>/dev/null)\`  IO线程: $THREADS  机器: ${TOTAL}核"
    echo "- 服务端绑核 \`$SRV_CORES\` / 压测端绑核 \`$CLI_CORES\`，构建 \`-O2 -DLOG_LEVEL=99\`"
    echo ""
    echo "| 标签 | 场景 | 并发 | 目标RPS | QPS | P50 | P99 | P99.9 | 服务端CPU |"
    echo "|---|---|---|---|---|---|---|---|---|"
} >>"$OUT"

echo ""
echo "--- keep-alive 扫速率（找 P99 开始劣化的拐点，那就是你的容量）---"
for R in $RATES; do
    run_one "keep-alive" "$R" -H "Connection: keep-alive"
done

echo ""
echo "--- 短连接（Connection: close，瓶颈完全不同：connect/close 开销主导）---"
for R in ${CLOSE_RATES:-1000 2000 5000}; do
    run_one "close" "$R" -H "Connection: close"
done

echo ""
echo "结果已追加到 $OUT"
echo "服务端日志尾部："
tail -5 "$SRV_LOG"
