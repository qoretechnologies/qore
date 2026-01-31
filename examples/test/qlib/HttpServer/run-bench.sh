#!/bin/bash
#
# Benchmark driver for async HTTP I/O.
#
# Runs wrk against the Qore HttpServer in async and sync modes to
# compare throughput and latency.
#
# Usage:
#   cd <qore-root>
#   examples/test/qlib/HttpServer/run-bench.sh [wrk-duration]
#
# wrk-duration defaults to 10 (seconds).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
QORE_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
export QORE_MODULE_DIR="$QORE_ROOT/qlib"

WRK="${WRK:-wrk}"
QORE="${QORE:-qore}"
DURATION="${1:-10}"

# wrk parameters
WRK_THREADS=4
CONNECTIONS_LIST="10 50 200"

run_one() {
    local label="$1"
    local extra_args="$2"
    local connections="$3"

    local port_file
    port_file=$(mktemp /tmp/bench-port.XXXXXX)
    rm -f "$port_file"

    # Start server; it writes the port to the file
    "$QORE" "$SCRIPT_DIR/bench-async-http.q" --duration=300 --quiet \
        --port-file="$port_file" $extra_args 2>/dev/null &
    local server_pid=$!

    # Wait for port file to appear
    local port=""
    for i in $(seq 1 100); do
        if [[ -s "$port_file" ]]; then
            port=$(cat "$port_file" | tr -d '[:space:]')
            [[ -n "$port" ]] && break
        fi
        sleep 0.1
    done
    rm -f "$port_file"

    if [[ -z "$port" ]]; then
        echo "ERROR: server did not start ($label)" >&2
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
        return 1
    fi

    # Warmup (2s)
    "$WRK" -t"$WRK_THREADS" -c"$connections" -d2s \
        "http://127.0.0.1:${port}/" > /dev/null 2>&1 || true

    # Benchmark
    local output
    output=$("$WRK" -t"$WRK_THREADS" -c"$connections" -d"${DURATION}s" --latency \
        "http://127.0.0.1:${port}/" 2>&1)

    # Extract metrics
    local rps lat_avg lat_p99 errors
    rps=$(printf '%s\n' "$output" | grep "Requests/sec:" | awk '{print $2}')
    lat_avg=$(printf '%s\n' "$output" | grep "Latency" | head -1 | awk '{print $2}')
    lat_p99=$(printf '%s\n' "$output" | awk '/Latency Distribution/{found=1} found && /99%/{print $2; exit}')
    errors=$(printf '%s\n' "$output" | grep "Socket errors:" | head -1 || true)

    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true

    printf "%-14s %-14s %-16s %-14s %-14s\n" "$label" "$connections" "$rps" "$lat_avg" "${lat_p99:--}"
    if [[ -n "$errors" ]]; then
        echo "  $errors"
    fi
}

echo "============================================="
echo "  Async HTTP I/O Benchmark"
echo "  wrk: $WRK_THREADS threads, ${DURATION}s per run"
echo "  $(date)"
echo "============================================="
echo ""
printf "%-14s %-14s %-16s %-14s %-14s\n" "Mode" "Connections" "Requests/sec" "Avg Latency" "p99 Latency"
printf "%-14s %-14s %-16s %-14s %-14s\n" "----------" "----------" "------------" "-----------" "-----------"

for conns in $CONNECTIONS_LIST; do
    run_one "async" "" "$conns"
done

for conns in $CONNECTIONS_LIST; do
    run_one "sync" "--sync" "$conns"
done

echo ""
echo "Done."
