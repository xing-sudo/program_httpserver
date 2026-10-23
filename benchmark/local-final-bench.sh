#!/usr/bin/env bash
set -u

SERVER_PID=$(cat server-final.pid)
OUT_DIR="local-final-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT_DIR"

RATES="${RATES:-30000 40000 50000 55000 60000}"

cleanup_round()
{
    for pid in "$@"; do
        kill "$pid" 2>/dev/null || true
    done
}

for rate in $RATES; do
    echo "===== testing RPS $rate ====="

    WRK_LOG="$OUT_DIR/wrk2-${rate}.log"
    WRK_PIDSTAT="$OUT_DIR/wrk2-pidstat-${rate}.log"
    WRK_MPSTAT="$OUT_DIR/wrk2-mpstat-${rate}.log"
    SERVER_PIDSTAT="$OUT_DIR/server-pidstat-${rate}.log"
    SERVER_MPSTAT="$OUT_DIR/server-mpstat-${rate}.log"

    taskset -c 2-3 wrk2 \
        -t2 -c1000 -d30s -R"$rate" --latency \
        -H 'Connection: keep-alive' \
        http://127.0.0.1:8080/hello \
        > "$WRK_LOG" 2>&1 &

    WRK_PID=$!

    pidstat -p "$WRK_PID" -u -r -d -t 1 \
        > "$WRK_PIDSTAT" 2>&1 &
    WRK_PIDSTAT_PID=$!

    mpstat -P 2,3 1 \
        > "$WRK_MPSTAT" 2>&1 &
    WRK_MPSTAT_PID=$!

    pidstat -p "$SERVER_PID" -u -r -t 1 \
        > "$SERVER_PIDSTAT" 2>&1 &
    SERVER_PIDSTAT_PID=$!

    mpstat -P 0,1 1 \
        > "$SERVER_MPSTAT" 2>&1 &
    SERVER_MPSTAT_PID=$!

    wait "$WRK_PID"

    cleanup_round \
        "$WRK_PIDSTAT_PID" \
        "$WRK_MPSTAT_PID" \
        "$SERVER_PIDSTAT_PID" \
        "$SERVER_MPSTAT_PID"

    sleep 5
done

echo "results saved in $OUT_DIR"