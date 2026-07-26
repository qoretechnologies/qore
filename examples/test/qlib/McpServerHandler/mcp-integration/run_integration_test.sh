#!/bin/bash
#
# MCP Integration Test Runner
#
# This script:
# 1. Starts the Qore MCP test server
# 2. Runs the Python MCP SDK compliance tests with both transports
# 3. Cleans up
#
# Requirements:
#   - qore with McpServerHandler module
#   - Python 3.10+ with 'mcp' and 'httpx' packages
#
# Usage:
#   ./run_integration_test.sh [--install-deps] [--transport sse|streamable|both]
#
# Environment:
#   PYTHON - Python interpreter to use (default: auto-detect python3.14, python3.12, etc.)
#   PIP - Pip to use (default: pip for $PYTHON)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODULE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
PORT_FILE="/tmp/mcp_test_port_$$"
SERVER_PID=""
TRANSPORT="both"

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
    rm -f "$PORT_FILE" "$SERVER_LOG"
}

trap cleanup EXIT

# Parse arguments
while [ $# -gt 0 ]; do
    case "$1" in
        --install-deps)
            echo -e "${YELLOW}Installing Python dependencies...${NC}"
            $PIP install --quiet mcp httpx
            ;;
        --transport)
            shift
            TRANSPORT="$1"
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
if ! $PYTHON -c "import mcp" 2>/dev/null; then
    echo -e "${RED}ERROR: MCP Python SDK not installed${NC}"
    echo "Run: $PIP install mcp httpx"
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

# Start the MCP test server
echo -e "${YELLOW}Starting MCP test server...${NC}"
SERVER_LOG="/tmp/mcp_server_$$.log"
export MCP_PORT_FILE="$PORT_FILE"
qore "$SCRIPT_DIR/mcp_test_server.q" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!

# Wait for server to start and write port
echo -e "${YELLOW}Waiting for server to start...${NC}"
WAIT_COUNT=0
MAX_WAIT=60
while [ ! -f "$PORT_FILE" ] && [ $WAIT_COUNT -lt $MAX_WAIT ]; do
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo -e "${RED}ERROR: Server process died${NC}"
        echo -e "${RED}Server log:${NC}"
        cat "$SERVER_LOG" 2>/dev/null
        exit 1
    fi
    sleep 1
    WAIT_COUNT=$((WAIT_COUNT + 1))
done

if [ ! -f "$PORT_FILE" ]; then
    echo -e "${RED}ERROR: Server did not start within ${MAX_WAIT}s${NC}"
    echo -e "${RED}Server log:${NC}"
    cat "$SERVER_LOG" 2>/dev/null
    exit 1
fi

PORT=$(cat "$PORT_FILE")
echo -e "${GREEN}Server started on port $PORT (PID: $SERVER_PID)${NC}"

# Give server a moment to fully initialize
sleep 1

# Run the Python compliance tests
SERVER_URL="http://localhost:$PORT"
OVERALL_EXIT=0

run_transport_test() {
    local transport="$1"
    echo -e "\n${YELLOW}========================================${NC}"
    echo -e "${YELLOW}Running MCP SDK compliance tests ($transport transport)...${NC}"
    echo -e "${YELLOW}========================================${NC}"

    if $PYTHON "$SCRIPT_DIR/test_mcp_compliance.py" "$SERVER_URL" --transport "$transport"; then
        echo -e "${GREEN}$transport transport tests PASSED${NC}"
        return 0
    else
        echo -e "${RED}$transport transport tests FAILED${NC}"
        return 1
    fi
}

case "$TRANSPORT" in
    sse)
        run_transport_test "sse" || OVERALL_EXIT=1
        ;;
    streamable)
        run_transport_test "streamable" || OVERALL_EXIT=1
        ;;
    both)
        run_transport_test "streamable" || OVERALL_EXIT=1
        run_transport_test "sse" || OVERALL_EXIT=1
        ;;
    *)
        echo -e "${RED}Unknown transport: $TRANSPORT${NC}"
        exit 1
        ;;
esac

echo ""
if [ $OVERALL_EXIT -eq 0 ]; then
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}All MCP compliance tests PASSED${NC}"
    echo -e "${GREEN}========================================${NC}"
else
    echo -e "${RED}========================================${NC}"
    echo -e "${RED}Some MCP compliance tests FAILED${NC}"
    echo -e "${RED}========================================${NC}"
fi

exit $OVERALL_EXIT
