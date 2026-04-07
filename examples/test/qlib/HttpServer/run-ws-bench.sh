#!/bin/bash
#
# WebSocket echo benchmark driver.
#
# Runs the Go ws-bench client against the Qore WebSocket echo server
# at various concurrency levels.
#
# Usage:
#   cd <qore-root>
#   examples/test/qlib/HttpServer/run-ws-bench.sh [messages-per-conn]
#
# messages-per-conn defaults to 5000.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
QORE_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
export QORE_MODULE_DIR="$QORE_ROOT/qlib"

QORE="${QORE:-qore}"
WS_BENCH="$SCRIPT_DIR/ws-bench/ws-bench"
MESSAGES="${1:-5000}"

# Build ws-bench if needed
if [[ ! -x "$WS_BENCH" ]]; then
    echo "Building ws-bench..."
    (cd "$SCRIPT_DIR/ws-bench" && go build -o ws-bench .) || {
        echo "ERROR: failed to build ws-bench (requires Go + gorilla/websocket)" >&2
        exit 1
    }
fi

CONNECTIONS_LIST="1 10 50 200"
PAYLOAD_SIZES="128 1024"

echo "============================================="
echo "  WebSocket Echo Benchmark"
echo "  $MESSAGES messages per connection"
echo "  $(date)"
echo "============================================="

for size in $PAYLOAD_SIZES; do
    echo ""
    echo "--- Payload: ${size} bytes ---"
    printf "%-14s %-16s %-14s %-14s %-14s\n" \
        "Connections" "Messages/sec" "Avg Latency" "p99 Latency" "Errors"
    printf "%-14s %-16s %-14s %-14s %-14s\n" \
        "----------" "------------" "-----------" "-----------" "------"

    for conns in $CONNECTIONS_LIST; do
        port_file=$(mktemp /tmp/ws-bench-port.XXXXXX)
        rm -f "$port_file"

        # Start server
        "$QORE" "$SCRIPT_DIR/bench-async-ws.q" --duration=300 --quiet \
            --port-file="$port_file" 2>/dev/null &
        server_pid=$!

        # Wait for port
        port=""
        for i in $(seq 1 100); do
            if [[ -s "$port_file" ]]; then
                port=$(cat "$port_file" | tr -d '[:space:]')
                [[ -n "$port" ]] && break
            fi
            sleep 0.1
        done
        rm -f "$port_file"

        if [[ -z "$port" ]]; then
            echo "ERROR: server did not start (conns=$conns)" >&2
            kill "$server_pid" 2>/dev/null || true
            wait "$server_pid" 2>/dev/null || true
            continue
        fi

        # Run benchmark
        output=$("$WS_BENCH" \
            -url "ws://127.0.0.1:${port}/ws" \
            -c "$conns" \
            -n "$MESSAGES" \
            -size "$size" \
            -warmup 100 2>/dev/null)

        # Extract metrics
        rps=$(echo "$output" | grep "Messages/sec:" | awk '{print $2}')
        lat_avg=$(echo "$output" | grep "Avg latency:" | awk '{print $3}')
        lat_p99=$(echo "$output" | grep "p99 latency:" | awk '{print $3}')
        errors=$(echo "$output" | grep "Errors:" | awk '{print $2}')

        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true

        printf "%-14s %-16s %-14s %-14s %-14s\n" \
            "$conns" "${rps:--}" "${lat_avg:--}" "${lat_p99:--}" "${errors:-0}"
    done
done

echo ""
echo "Done."
