#!/bin/bash
#
# Copyright (C) 2026 Qore Technologies, s.r.o., all rights reserved
#
# System Contention Testing with stress-ng
# -----------------------------------------
# Runs stress-ng in the background while executing the AsyncIoController
# test suite, then compares results with a baseline run.
#
# Usage:
#   ./run-stress-ng-bench.sh [--qore=PATH] [--duration=SECS] [--verbose]
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../../.." && pwd)"

# Defaults
QORE="${QORE:-}"
DURATION="${DURATION:-30}"
VERBOSE="${VERBOSE:-}"
STRESS_PID=""

usage() {
    cat <<'EOF'
Usage: run-stress-ng-bench.sh [OPTIONS]

Runs asyncio_stress.qtest under various system stressors and compares results.

Options:
  --qore=PATH       Path to qore binary (default: auto-detect)
  --duration=SECS   Duration for each stress test (default: 30)
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
        --qore=*)     QORE="${arg#--qore=}" ;;
        --duration=*) DURATION="${arg#--duration=}" ;;
        --verbose)    VERBOSE=1 ;;
        --help)       usage ;;
        *)            echo "Unknown option: $arg"; usage ;;
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

# Check for stress-ng
if ! command -v stress-ng >/dev/null 2>&1; then
    cat <<'EOF'
ERROR: stress-ng not found. Install with:
  Ubuntu/Debian: sudo apt-get install stress-ng
  Fedora/RHEL:   sudo dnf install stress-ng
  macOS:         brew install stress-ng
EOF
    exit 1
fi

echo "stress-ng: $(command -v stress-ng)"

TEST_FILE="$SCRIPT_DIR/asyncio_stress.qtest"
if [ ! -f "$TEST_FILE" ]; then
    echo "ERROR: Test file not found: $TEST_FILE"
    exit 1
fi

echo "Test file: $TEST_FILE"
echo "Duration per stressor: ${DURATION}s"

# Set up module path
export QORE_MODULE_DIR="${QORE_MODULE_DIR:-$REPO_ROOT/qlib}"

# Cleanup handler
cleanup() {
    if [ -n "$STRESS_PID" ] && kill -0 "$STRESS_PID" 2>/dev/null; then
        kill "$STRESS_PID" 2>/dev/null || true
        wait "$STRESS_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "=== System Contention Testing ==="
echo "CPUs: $NPROC"
echo ""

printf "%-20s %8s %8s %10s %10s\n" "Stressor" "Tests" "Status" "Time" "vs Baseline"
printf "%-20s %8s %8s %10s %10s\n" "--------" "-----" "------" "----" "-----------"

BASELINE_TIME=""

# Run the test suite and capture results
# Returns: sets LAST_TESTS, LAST_STATUS, LAST_TIME
run_tests() {
    local label="$1"
    local start_time end_time elapsed

    start_time=$(date +%s%N)

    local output exit_code=0
    output=$("$QORE" --enable-debug "$TEST_FILE" 2>&1) || exit_code=$?

    end_time=$(date +%s%N)
    elapsed=$(( (end_time - start_time) / 1000000 ))  # milliseconds
    LAST_TIME="$elapsed"

    # Parse test count from output
    # QUnit output format: "Ran N test(s), M assertion(s)"
    local test_count
    test_count=$(echo "$output" | grep -oP 'Ran \K\d+(?= test)') || test_count="?"

    if [ "$exit_code" -eq 0 ]; then
        LAST_STATUS="PASS"
        LAST_TESTS="${test_count}/${test_count}"
    else
        LAST_STATUS="FAIL"
        # Try to extract failure count
        local failures
        failures=$(echo "$output" | grep -oP '\d+(?= error)') || failures="?"
        LAST_TESTS="${test_count}/${test_count}"
        if [ "$failures" != "?" ] && [ "$failures" != "0" ]; then
            LAST_STATUS="FAIL($failures)"
        fi
    fi

    if [ -n "$VERBOSE" ]; then
        echo "--- $label output ---"
        echo "$output"
        echo "---"
    fi
}

# Format time in seconds with one decimal
format_time() {
    local ms="$1"
    # Use awk for floating point
    echo "$ms" | awk '{printf "%.1fs", $1/1000}'
}

# Format speedup ratio
format_ratio() {
    local current="$1"
    local baseline="$2"
    if [ "$baseline" = "0" ] || [ -z "$baseline" ]; then
        echo "N/A"
        return
    fi
    echo "$current $baseline" | awk '{printf "%.1fx", $1/$2}'
}

# 1. Baseline (no stressor)
run_tests "baseline"
BASELINE_TIME="$LAST_TIME"
printf "%-20s %8s %8s %10s %10s\n" "baseline" "$LAST_TESTS" "$LAST_STATUS" \
    "$(format_time "$LAST_TIME")" "1.0x"

# 2. CPU stress
STRESS_PID=""
stress-ng --cpu "$NPROC" --timeout "${DURATION}s" --quiet &
STRESS_PID=$!

run_tests "cpu-stress"
printf "%-20s %8s %8s %10s %10s\n" "cpu-stress" "$LAST_TESTS" "$LAST_STATUS" \
    "$(format_time "$LAST_TIME")" "$(format_ratio "$LAST_TIME" "$BASELINE_TIME")"

# Wait for stress-ng to finish (it may already be done if tests took longer)
if kill -0 "$STRESS_PID" 2>/dev/null; then
    kill "$STRESS_PID" 2>/dev/null || true
    wait "$STRESS_PID" 2>/dev/null || true
fi
STRESS_PID=""

# 3. I/O stress
stress-ng --io "$NPROC" --timeout "${DURATION}s" --quiet &
STRESS_PID=$!

run_tests "io-stress"
printf "%-20s %8s %8s %10s %10s\n" "io-stress" "$LAST_TESTS" "$LAST_STATUS" \
    "$(format_time "$LAST_TIME")" "$(format_ratio "$LAST_TIME" "$BASELINE_TIME")"

if kill -0 "$STRESS_PID" 2>/dev/null; then
    kill "$STRESS_PID" 2>/dev/null || true
    wait "$STRESS_PID" 2>/dev/null || true
fi
STRESS_PID=""

# 4. Context-switch storm
stress-ng --context 0 --timeout "${DURATION}s" --quiet &
STRESS_PID=$!

run_tests "context-switch"
printf "%-20s %8s %8s %10s %10s\n" "context-switch" "$LAST_TESTS" "$LAST_STATUS" \
    "$(format_time "$LAST_TIME")" "$(format_ratio "$LAST_TIME" "$BASELINE_TIME")"

if kill -0 "$STRESS_PID" 2>/dev/null; then
    kill "$STRESS_PID" 2>/dev/null || true
    wait "$STRESS_PID" 2>/dev/null || true
fi
STRESS_PID=""

# 5. FD / socket pressure
stress-ng --sock "$NPROC" --sock-ops 1000000 --timeout "${DURATION}s" --quiet &
STRESS_PID=$!

run_tests "fd-pressure"
printf "%-20s %8s %8s %10s %10s\n" "fd-pressure" "$LAST_TESTS" "$LAST_STATUS" \
    "$(format_time "$LAST_TIME")" "$(format_ratio "$LAST_TIME" "$BASELINE_TIME")"

if kill -0 "$STRESS_PID" 2>/dev/null; then
    kill "$STRESS_PID" 2>/dev/null || true
    wait "$STRESS_PID" 2>/dev/null || true
fi
STRESS_PID=""

echo ""
echo "=== Contention Testing Complete ==="
