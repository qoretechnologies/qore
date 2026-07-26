#!/bin/bash
#
# A2A Integration Test Runner
#
# This script validates our A2A implementation against known-good external
# implementations in two directions:
#
#   1. Server validation: Starts our Qore A2A server and runs Python compliance
#      tests against it (validates our A2aServerHandler)
#
#   2. Client validation: Starts a Python A2A reference server and runs our
#      Qore A2A client tests against it (validates our A2aClient)
#
# Requirements:
#   - qore with A2aServerHandler and A2aClient modules
#   - Python 3.10+ with 'httpx' package
#
# Usage:
#   ./run_integration_test.sh [--install-deps] [--test server|client|both]
#
# Environment:
#   PYTHON - Python interpreter to use (default: auto-detect)
#   PIP - Pip to use (default: pip for $PYTHON)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODULE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
SERVER_PID=""
REF_SERVER_PID=""
TEST_MODE="both"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Find Python 3.10+ if not specified
if [ -z "$PYTHON" ]; then
    for py in python3.14 python3.13 python3.12 python3.11 python3.10; do
        if command -v "$py" &>/dev/null; then
            PYTHON="$py"
            break
        fi
    done
    if [ -z "$PYTHON" ]; then
        echo -e "${RED}ERROR: Python 3.10+ required but not found${NC}"
        exit 2
    fi
fi

# Find pip for the Python version
if [ -z "$PIP" ]; then
    PY_VERSION=$($PYTHON -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
    for pip_cmd in "pip${PY_VERSION}" "$PYTHON -m pip" "pip3"; do
        if $pip_cmd --version &>/dev/null 2>&1; then
            PIP="$pip_cmd"
            break
        fi
    done
fi

echo -e "${YELLOW}Using Python: $PYTHON${NC}"
echo -e "${YELLOW}Using pip: $PIP${NC}"

cleanup() {
    echo -e "${YELLOW}Cleaning up...${NC}"
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    [ -n "$A2A_PORT_FILE" ] && rm -f "$A2A_PORT_FILE"
    [ -n "$SERVER_LOG" ] && rm -f "$SERVER_LOG"
}

trap cleanup EXIT

# Parse arguments
while [ $# -gt 0 ]; do
    case "$1" in
        --install-deps)
            echo -e "${YELLOW}Installing Python dependencies...${NC}"
            $PIP install --quiet httpx
            ;;
        --test)
            shift
            TEST_MODE="$1"
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
    shift
done

# Check Python dependencies
echo -e "${YELLOW}Checking Python dependencies...${NC}"
if ! $PYTHON -c "import httpx" 2>/dev/null; then
    echo -e "${RED}ERROR: httpx not installed${NC}"
    echo "Run: $PIP install httpx"
    echo "Or: $0 --install-deps"
    exit 2
fi

# Check Qore
echo -e "${YELLOW}Checking Qore...${NC}"
if ! command -v qore &>/dev/null; then
    echo -e "${RED}ERROR: qore not found in PATH${NC}"
    exit 2
fi

# Set up module path
export QORE_MODULE_DIR="${MODULE_DIR}/qlib:${QORE_MODULE_DIR}"

OVERALL_EXIT=0

# ================================================================
# Part 1: Self-tests (informational, non-blocking)
# These use our own Python test code — useful for broad coverage
# but not authoritative (could have matching bugs on both sides).
# The SDK interop tests in Part 2 are the authoritative validation.
# ================================================================
run_server_tests() {
    echo -e "\n${YELLOW}========================================${NC}"
    echo -e "${YELLOW}Part 1: Self-Tests (informational)${NC}"
    echo -e "${YELLOW}Testing with our own Python test code${NC}"
    echo -e "${YELLOW}========================================${NC}"

    A2A_PORT_FILE="/tmp/a2a_server_port_$$"
    SERVER_LOG="/tmp/a2a_server_$$.log"

    # Start our Qore A2A test server
    echo -e "${YELLOW}Starting Qore A2A test server...${NC}"
    export A2A_PORT_FILE
    qore "$SCRIPT_DIR/a2a_test_server.q" >"$SERVER_LOG" 2>&1 &
    SERVER_PID=$!

    # Wait for server to start
    echo -e "${YELLOW}Waiting for server to start...${NC}"
    WAIT_COUNT=0
    MAX_WAIT=60
    while [ ! -f "$A2A_PORT_FILE" ] && [ $WAIT_COUNT -lt $MAX_WAIT ]; do
        if ! kill -0 "$SERVER_PID" 2>/dev/null; then
            echo -e "${RED}ERROR: Server process died${NC}"
            echo -e "${RED}Server log:${NC}"
            cat "$SERVER_LOG" 2>/dev/null
            return 1
        fi
        sleep 1
        WAIT_COUNT=$((WAIT_COUNT + 1))
    done

    if [ ! -f "$A2A_PORT_FILE" ]; then
        echo -e "${RED}ERROR: Server did not start within ${MAX_WAIT}s${NC}"
        echo -e "${RED}Server log:${NC}"
        cat "$SERVER_LOG" 2>/dev/null
        return 1
    fi

    local port=$(cat "$A2A_PORT_FILE")
    echo -e "${GREEN}Qore A2A server started on port $port (PID: $SERVER_PID)${NC}"
    sleep 1

    # Run Python compliance tests against our server (both v0.3 and v1.0)
    local server_url="http://localhost:$port"
    if $PYTHON "$SCRIPT_DIR/test_a2a_compliance.py" "$server_url" --version both; then
        echo -e "${GREEN}Server validation PASSED${NC}"
        local result=0
    else
        echo -e "${RED}Server validation FAILED${NC}"
        local result=1
    fi

    # Stop server
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
    SERVER_PID=""
    rm -f "$A2A_PORT_FILE" "$SERVER_LOG"

    return $result
}

# ================================================================
# Part 2: SDK Interop (replaces Python reference server)
# Tests against the official a2a-sdk — the only independent implementation
# ================================================================
run_sdk_interop_tests() {
    echo -e "\n${YELLOW}========================================${NC}"
    echo -e "${YELLOW}Part 2: SDK Interoperability${NC}"
    echo -e "${YELLOW}Testing against official a2a-sdk${NC}"
    echo -e "${YELLOW}========================================${NC}"

    if ! $PYTHON -c "import a2a" 2>/dev/null; then
        echo -e "${RED}ERROR: a2a-sdk not installed — SDK interop tests required${NC}"
        return 1
    fi

    # Start our Qore server for SDK tests
    A2A_PORT_FILE="/tmp/a2a_sdk_port_$$"
    SERVER_LOG="/tmp/a2a_sdk_server_$$.log"
    export A2A_PORT_FILE
    qore "$SCRIPT_DIR/a2a_test_server.q" >"$SERVER_LOG" 2>&1 &
    SERVER_PID=$!

    WAIT_COUNT=0
    MAX_WAIT=60
    while [ ! -f "$A2A_PORT_FILE" ] && [ $WAIT_COUNT -lt $MAX_WAIT ]; do
        if ! kill -0 "$SERVER_PID" 2>/dev/null; then
            echo -e "${RED}ERROR: Server process died${NC}"
            echo -e "${RED}Server log:${NC}"
            cat "$SERVER_LOG" 2>/dev/null
            return 1
        fi
        sleep 1
        WAIT_COUNT=$((WAIT_COUNT + 1))
    done

    if [ ! -f "$A2A_PORT_FILE" ]; then
        echo -e "${RED}ERROR: Server did not start within ${MAX_WAIT}s${NC}"
        echo -e "${RED}Server log:${NC}"
        cat "$SERVER_LOG" 2>/dev/null
        return 1
    fi

    local port=$(cat "$A2A_PORT_FILE")
    echo -e "${GREEN}Qore A2A server started on port $port (PID: $SERVER_PID)${NC}"

    local sdk_test_port=$($PYTHON -c "import socket; s=socket.socket(); s.bind(('',0)); print(s.getsockname()[1]); s.close()")
    if $PYTHON "$SCRIPT_DIR/test_a2a_sdk_interop.py" \
            "http://localhost:$port" \
            --sdk-port "$sdk_test_port"; then
        echo -e "${GREEN}SDK interop PASSED${NC}"
        local result=0
    else
        echo -e "${RED}SDK interop FAILED${NC}"
        local result=1
    fi

    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
    SERVER_PID=""
    rm -f "$A2A_PORT_FILE" "$SERVER_LOG"

    return $result
}

# ================================================================
# Run tests based on mode
# ================================================================
case "$TEST_MODE" in
    server)
        run_server_tests || OVERALL_EXIT=1
        ;;
    client)
        run_sdk_interop_tests || OVERALL_EXIT=1
        ;;
    both)
        # Part 1: Self-tests (informational — failures logged but non-blocking)
        if ! run_server_tests; then
            echo -e "${YELLOW}Self-tests had failures (informational only)${NC}"
        fi
        # Part 2: SDK interop (authoritative — failures are blocking)
        run_sdk_interop_tests || OVERALL_EXIT=1
        ;;
    *)
        echo -e "${RED}Unknown test mode: $TEST_MODE${NC}"
        exit 1
        ;;
esac

echo ""
if [ $OVERALL_EXIT -eq 0 ]; then
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}All A2A integration tests PASSED${NC}"
    echo -e "${GREEN}========================================${NC}"
else
    echo -e "${RED}========================================${NC}"
    echo -e "${RED}Some A2A integration tests FAILED${NC}"
    echo -e "${RED}========================================${NC}"
fi

exit $OVERALL_EXIT
