#!/bin/bash
#
# Copyright (C) 2026 Qore Technologies, s.r.o., all rights reserved
#
# HTTP/2 Load Testing with h2load
# --------------------------------
# Starts a Qore HTTP/2 server and runs h2load against it with varying
# parameters. Also supports wrk for HTTP/1.1 comparison.
#
# Usage:
#   ./run-h2load-bench.sh [--qore=PATH] [--requests=N] [--verbose]
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../../.." && pwd)"

# Defaults
QORE="${QORE:-}"
REQUESTS="${REQUESTS:-100000}"
VERBOSE="${VERBOSE:-}"
SERVER_PID=""
PORT_FILE=""

usage() {
    cat <<'EOF'
Usage: run-h2load-bench.sh [OPTIONS]

Options:
  --qore=PATH       Path to qore binary (default: auto-detect)
  --requests=N      Total requests per test (default: 100000)
  --verbose         Verbose output
  --help            Show usage

Environment:
  LD_LIBRARY_PATH   Set to build/ directory if testing uninstalled build
  QORE_MODULE_DIR   Set if modules are not installed
EOF
    exit 0
}

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        --qore=*)   QORE="${arg#--qore=}" ;;
        --requests=*) REQUESTS="${arg#--requests=}" ;;
        --verbose)  VERBOSE=1 ;;
        --help)     usage ;;
        *)          echo "Unknown option: $arg"; usage ;;
    esac
done

# Auto-detect qore binary
if [ -z "$QORE" ]; then
    if [ -x "$REPO_ROOT/build/qore" ]; then
        QORE="$REPO_ROOT/build/qore"
        export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}:$REPO_ROOT/build"
    elif command -v qore >/dev/null 2>&1; then
        QORE="$(command -v qore)"
    else
        echo "ERROR: qore binary not found. Set --qore=PATH or build in $REPO_ROOT/build/"
        exit 1
    fi
fi

echo "Using qore: $QORE"

# Check for h2load
if ! command -v h2load >/dev/null 2>&1; then
    cat <<'EOF'
ERROR: h2load not found. Install with:
  Ubuntu/Debian: sudo apt-get install nghttp2-client
  Fedora/RHEL:   sudo dnf install nghttp2
  macOS:         brew install nghttp2
EOF
    exit 1
fi

echo "h2load: $(command -v h2load)"

# Check for wrk (optional)
HAS_WRK=0
if command -v wrk >/dev/null 2>&1; then
    HAS_WRK=1
    echo "wrk:    $(command -v wrk)"
else
    echo "wrk:    not found (HTTP/1.1 comparison will be skipped)"
    echo "        Install with: sudo apt-get install wrk  OR  brew install wrk"
fi

# Cleanup handler
cleanup() {
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "Stopping server (PID $SERVER_PID)..."
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    if [ -n "$PORT_FILE" ] && [ -f "$PORT_FILE" ]; then
        rm -f "$PORT_FILE"
    fi
}
trap cleanup EXIT

# Start the HTTP/2 benchmark server
PORT_FILE="$(mktemp /tmp/h2-bench-port.XXXXXX)"

echo ""
echo "Starting HTTP/2 benchmark server..."

QORE_MODULE_DIR="${QORE_MODULE_DIR:-$REPO_ROOT/qlib}" \
    "$QORE" "$SCRIPT_DIR/h2-bench-server.qr" --port-file="$PORT_FILE" \
    ${VERBOSE:+--verbose} &
SERVER_PID=$!

# Wait for the server to write its port
for i in $(seq 1 30); do
    if [ -s "$PORT_FILE" ]; then
        break
    fi
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "ERROR: Server failed to start"
        wait "$SERVER_PID" 2>/dev/null || true
        exit 1
    fi
    sleep 0.1
done

PORT="$(cat "$PORT_FILE" | tr -d '[:space:]')"
if [ -z "$PORT" ]; then
    echo "ERROR: Failed to get server port"
    exit 1
fi

URL="https://localhost:$PORT/"
echo "Server running on port $PORT (PID $SERVER_PID)"

# Give server a moment to fully initialize
sleep 0.5

echo ""
echo "=== HTTP/2 Load Test (h2load) ==="
echo "Server: $URL"
echo "Requests per test: $REQUESTS"
echo ""

# Column header
printf "%-35s %12s %12s %12s\n" "Test" "Req/sec" "Time(s)" "Status"
printf "%-35s %12s %12s %12s\n" "---" "-------" "-------" "------"

# Run h2load and extract metrics
run_h2load() {
    local label="$1"
    local connections="$2"
    local streams="$3"
    local requests="$4"

    local output
    output=$(h2load -n"$requests" -c"$connections" -m"$streams" \
        --h2 "$URL" 2>&1) || true

    # Extract req/sec and time from h2load output (portable, no grep -P)
    local req_sec time_sec status

    # h2load outputs "req/s: <num>" — extract the number after "req/s:"
    req_sec=$(echo "$output" | awk '/req\/s:/ {for(i=1;i<=NF;i++) if($i=="req/s:") {print $(i+1); exit}}') || req_sec=""
    if [ -z "$req_sec" ]; then
        # Alternative: "NNN requests/sec" format
        req_sec=$(echo "$output" | awk '/requests\/sec/ {print $1}') || req_sec=""
    fi
    : "${req_sec:=N/A}"

    # h2load outputs "finished in Xs, ..." — extract the time
    time_sec=$(echo "$output" | awk '/^finished in/ {gsub(/[^0-9.]/, "", $3); print $3; exit}') || time_sec=""
    : "${time_sec:=N/A}"

    # Check for errors: "NNN succeeded, ..." line
    if echo "$output" | grep -q "succeeded"; then
        local succeeded
        succeeded=$(echo "$output" | awk '/succeeded/ {for(i=1;i<=NF;i++) if($(i+1)=="succeeded,") {print $i; exit}}') || succeeded="0"
        if [ "$succeeded" = "$requests" ]; then
            status="OK"
        else
            status="${succeeded}/${requests}"
        fi
    else
        status="ERR"
    fi

    printf "%-35s %12s %12s %12s\n" "$label" "$req_sec" "$time_sec" "$status"

    if [ -n "$VERBOSE" ]; then
        echo "--- h2load output ---"
        echo "$output"
        echo "---"
    fi
}

# Test matrix
run_h2load "1 conn, 1 stream"         1    1    "$REQUESTS"
run_h2load "1 conn, 10 streams"        1    10   "$REQUESTS"
run_h2load "1 conn, 100 streams"       1    100  "$REQUESTS"
run_h2load "10 conns, 10 streams"      10   10   "$REQUESTS"
run_h2load "50 conns, 10 streams"      50   10   "$REQUESTS"
run_h2load "100 conns, 1 stream"       100  1    "$REQUESTS"
run_h2load "100 conns, 10 streams"     100  10   "$REQUESTS"

echo ""

# HTTP/1.1 comparison with wrk (if available)
if [ "$HAS_WRK" = "1" ]; then
    echo "=== HTTP/1.1 Comparison (wrk) ==="
    echo ""

    printf "%-35s %12s %12s %12s\n" "Test" "Req/sec" "Latency" "Status"
    printf "%-35s %12s %12s %12s\n" "---" "-------" "-------" "------"

    run_wrk() {
        local label="$1"
        local threads="$2"
        local connections="$3"
        local duration="$4"

        local output
        output=$(wrk -t"$threads" -c"$connections" -d"$duration" "$URL" 2>&1) || true

        local req_sec latency status
        req_sec=$(echo "$output" | grep "Requests/sec" | awk '{print $2}') || req_sec="N/A"
        latency=$(echo "$output" | grep "Latency" | awk '{print $2}') || latency="N/A"

        if echo "$output" | grep -q "errors"; then
            status="ERRORS"
        else
            status="OK"
        fi

        printf "%-35s %12s %12s %12s\n" "$label" "$req_sec" "$latency" "$status"

        if [ -n "$VERBOSE" ]; then
            echo "--- wrk output ---"
            echo "$output"
            echo "---"
        fi
    }

    run_wrk "1 thread, 10 conns, 10s"   1  10   10s
    run_wrk "4 threads, 100 conns, 10s"  4  100  10s

    echo ""
fi

echo "=== Benchmark Complete ==="
