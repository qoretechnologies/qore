#!/bin/bash
#
# MCP Integration Test Runner
#
# Validates the Qore MCP implementation against the official MCP Python SDK, which is an
# independent implementation of the same specification.  Both directions are covered for
# every protocol revision the Qore modules support:
#
#   1. Qore server  <- SDK client   (test_mcp_compliance*.py)
#   2. SDK server   <- Qore client  (qore_client_compliance.qr)
#
# Each protocol revision is exercised by the SDK release that implements it as its own latest
# version, installed into its own virtual environment (the SDK majors are not co-installable):
#
#   protocol      SDK        transport
#   ----------    --------   ----------------------------
#   2024-11-05    1.6.0      HTTP+SSE (deprecated)
#   2025-03-26    1.9.4      Streamable HTTP
#   2025-06-18    1.20.0     Streamable HTTP
#   2025-11-25    1.29.0     Streamable HTTP
#   2026-07-28    2.0.0      Streamable HTTP (stateless)
#
# Requirements:
#   - qore with the McpServerHandler and McpClient modules
#   - Python 3.10+
#
# Usage:
#   ./run_integration_test.sh [--install-deps] [--protocol <version>|all]
#                             [--direction server|client|both]
#                             [--transport sse|streamable|both]
#
# Environment:
#   PYTHON  - Python interpreter to use (default: auto-detected)
#   VENVDIR - directory holding the per-protocol virtual environments (default: ./.venvs)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODULE_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
VENVDIR="${VENVDIR:-$SCRIPT_DIR/.venvs}"
PROTOCOL="all"
DIRECTION="both"
TRANSPORT="both"
INSTALL_DEPS=0
SERVER_PID=""
REF_PID=""

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# protocol:sdk-version:era
PROTOCOL_MATRIX=(
    "2024-11-05:1.6.0:legacy"
    "2025-03-26:1.9.4:legacy"
    "2025-06-18:1.20.0:legacy"
    "2025-11-25:1.29.0:legacy"
    "2026-07-28:2.0.0:modern"
)

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

# Terminates a process, escalating to SIGKILL: a reference server holding an open SSE
# response stream will not shut down gracefully, because its HTTP server waits for the
# long-lived response to finish first.
stop_pid() {
    local pid="$1" count=0
    [ -n "$pid" ] || return 0
    kill -0 "$pid" 2>/dev/null || return 0
    kill "$pid" 2>/dev/null || true
    while kill -0 "$pid" 2>/dev/null && [ $count -lt 10 ]; do
        sleep 0.5
        count=$((count + 1))
    done
    if kill -0 "$pid" 2>/dev/null; then
        kill -9 "$pid" 2>/dev/null || true
    fi
    wait "$pid" 2>/dev/null || true
}

cleanup() {
    for pid in "$SERVER_PID" "$REF_PID"; do
        stop_pid "$pid"
    done
    rm -f /tmp/mcp_port_$$ /tmp/mcp_ref_port_$$ /tmp/mcp_server_$$.log /tmp/mcp_ref_$$.log
}
trap cleanup EXIT

while [ $# -gt 0 ]; do
    case "$1" in
        --install-deps) INSTALL_DEPS=1 ;;
        --protocol) shift; PROTOCOL="$1" ;;
        --direction) shift; DIRECTION="$1" ;;
        --transport) shift; TRANSPORT="$1" ;;
        --era)
            # accepted for backwards compatibility with the previous interface
            shift
            case "$1" in
                modern) PROTOCOL="2026-07-28" ;;
                legacy) PROTOCOL="2025-11-25" ;;
                both) PROTOCOL="all" ;;
                *) echo "Unknown era: $1"; exit 1 ;;
            esac
            ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
    shift
done

case "$DIRECTION" in
    server|client|both) ;;
    *) echo -e "${RED}Unknown direction: $DIRECTION${NC}"; exit 1 ;;
esac

echo -e "${YELLOW}Using Python: $PYTHON${NC}"

venv_python() {
    echo "$VENVDIR/$1/bin/python"
}

setup_venv() {
    local proto="$1" sdk="$2" venv="$VENVDIR/$1"
    if [ ! -d "$venv" ]; then
        echo -e "${YELLOW}Creating venv for protocol $proto (mcp $sdk)...${NC}"
        $PYTHON -m venv "$venv"
        "$venv/bin/pip" install --quiet --upgrade pip
    fi
    echo -e "${YELLOW}Installing mcp==$sdk for protocol $proto...${NC}"
    "$venv/bin/pip" install --quiet "mcp==$sdk"
}

check_venv() {
    local proto="$1" sdk="$2" py
    py="$(venv_python "$proto")"
    if [ ! -x "$py" ] || ! "$py" -c "import mcp" 2>/dev/null; then
        echo -e "${RED}ERROR: no SDK installed for protocol $proto (expected mcp $sdk)${NC}"
        echo "Run: $0 --install-deps"
        return 1
    fi
    local have
    have="$("$py" -c 'import importlib.metadata as m; print(m.version("mcp"))')"
    if [ "$have" != "$sdk" ]; then
        echo -e "${RED}ERROR: protocol $proto venv has mcp $have, expected $sdk${NC}"
        return 1
    fi
    # confirm the SDK really implements the protocol revision it is pinned for
    local speaks
    speaks="$("$py" -c 'import mcp.types as t; print(t.LATEST_PROTOCOL_VERSION)' 2>/dev/null || echo unknown)"
    if [ "$speaks" != "$proto" ]; then
        echo -e "${RED}ERROR: mcp $have implements protocol $speaks, not $proto${NC}"
        return 1
    fi
    return 0
}

selected_rows() {
    local row proto
    for row in "${PROTOCOL_MATRIX[@]}"; do
        proto="${row%%:*}"
        if [ "$PROTOCOL" = "all" ] || [ "$PROTOCOL" = "$proto" ]; then
            echo "$row"
        fi
    done
}

if [ -z "$(selected_rows)" ]; then
    echo -e "${RED}Unknown protocol: $PROTOCOL${NC}"
    echo "Known protocols: $(for r in "${PROTOCOL_MATRIX[@]}"; do printf '%s ' "${r%%:*}"; done)"
    exit 1
fi

if [ "$INSTALL_DEPS" = "1" ]; then
    while IFS=: read -r proto sdk era; do
        setup_venv "$proto" "$sdk"
    done < <(selected_rows)
fi

echo -e "${YELLOW}Checking Python dependencies...${NC}"
while IFS=: read -r proto sdk era; do
    check_venv "$proto" "$sdk" || exit 2
    echo -e "${GREEN}protocol $proto: mcp $sdk${NC}"
done < <(selected_rows)

echo -e "${YELLOW}Checking Qore...${NC}"
if ! command -v qore &>/dev/null; then
    echo -e "${RED}ERROR: qore not found in PATH${NC}"
    exit 2
fi

export QORE_MODULE_DIR="${MODULE_DIR}/qlib:${QORE_MODULE_DIR}"

wait_for_port_file() {
    local file="$1" pid="$2" log="$3" what="$4" count=0
    while [ ! -f "$file" ] && [ $count -lt 60 ]; do
        if ! kill -0 "$pid" 2>/dev/null; then
            echo -e "${RED}ERROR: $what died${NC}"
            cat "$log" 2>/dev/null
            return 1
        fi
        sleep 1
        count=$((count + 1))
    done
    if [ ! -f "$file" ]; then
        echo -e "${RED}ERROR: $what did not start within 60s${NC}"
        cat "$log" 2>/dev/null
        return 1
    fi
    return 0
}

start_qore_server() {
    local port_file="/tmp/mcp_port_$$" log="/tmp/mcp_server_$$.log"
    rm -f "$port_file"
    MCP_PORT_FILE="$port_file" qore "$SCRIPT_DIR/mcp_test_server.q" >"$log" 2>&1 &
    SERVER_PID=$!
    wait_for_port_file "$port_file" "$SERVER_PID" "$log" "the Qore MCP server" || return 1
    QORE_SERVER_URL="http://localhost:$(cat "$port_file")"
    sleep 1
    return 0
}

stop_qore_server() {
    stop_pid "$SERVER_PID"
    SERVER_PID=""
}

start_reference_server() {
    local proto="$1" transport="$2"
    local port_file="/tmp/mcp_ref_port_$$" log="/tmp/mcp_ref_$$.log"
    rm -f "$port_file"
    "$(venv_python "$proto")" "$SCRIPT_DIR/sdk_reference_server.py" \
        --port-file "$port_file" --transport "$transport" >"$log" 2>&1 &
    REF_PID=$!
    wait_for_port_file "$port_file" "$REF_PID" "$log" "the SDK reference server" || return 1
    REF_SERVER_PORT="$(cat "$port_file")"
    sleep 1
    return 0
}

stop_reference_server() {
    stop_pid "$REF_PID"
    REF_PID=""
}

OVERALL_EXIT=0

banner() {
    echo -e "\n${YELLOW}========================================${NC}"
    echo -e "${YELLOW}$1${NC}"
    echo -e "${YELLOW}========================================${NC}"
}

# Direction 1: the SDK client drives the Qore server
run_server_direction() {
    local proto="$1" era="$2"
    local py
    py="$(venv_python "$proto")"

    if [ "$era" = "modern" ]; then
        banner "protocol $proto: SDK client -> Qore server"
        if "$py" "$SCRIPT_DIR/test_mcp_compliance_modern.py" "$QORE_SERVER_URL"; then
            echo -e "${GREEN}PASSED${NC}"
        else
            echo -e "${RED}FAILED${NC}"
            return 1
        fi
        return 0
    fi

    local transports=()
    case "$TRANSPORT" in
        sse) transports=("sse") ;;
        streamable) transports=("streamable") ;;
        both)
            # streamable HTTP did not exist before 2025-03-26
            if [ "$proto" = "2024-11-05" ]; then
                transports=("sse")
            else
                transports=("streamable" "sse")
            fi
            ;;
    esac

    local rc=0 t
    for t in "${transports[@]}"; do
        banner "protocol $proto: SDK client ($t) -> Qore server"
        if "$py" "$SCRIPT_DIR/test_mcp_compliance.py" "$QORE_SERVER_URL" --transport "$t"; then
            echo -e "${GREEN}PASSED${NC}"
        else
            echo -e "${RED}FAILED${NC}"
            rc=1
        fi
    done
    return $rc
}

# Direction 2: the Qore client drives the SDK reference server
run_client_direction() {
    local proto="$1" era="$2"
    local transport="streamable-http" url_path=""

    # 2024-11-05 predates Streamable HTTP; its transport is HTTP+SSE, where the client opens
    # the event stream at /sse and is told where to post
    if [ "$proto" = "2024-11-05" ]; then
        transport="sse"
        url_path="/sse"
    else
        url_path="/mcp"
    fi

    banner "protocol $proto: Qore client -> SDK reference server"
    if ! start_reference_server "$proto" "$transport"; then
        return 1
    fi
    local rc=0
    if qore "$SCRIPT_DIR/qore_client_compliance.qr" \
            "http://localhost:${REF_SERVER_PORT}${url_path}" "$proto"; then
        echo -e "${GREEN}PASSED${NC}"
    else
        echo -e "${RED}FAILED${NC}"
        rc=1
    fi
    stop_reference_server
    return $rc
}

if [ "$DIRECTION" = "server" ] || [ "$DIRECTION" = "both" ]; then
    if ! start_qore_server; then
        exit 1
    fi
    echo -e "${GREEN}Qore MCP server started at $QORE_SERVER_URL${NC}"
    while IFS=: read -r proto sdk era; do
        run_server_direction "$proto" "$era" || OVERALL_EXIT=1
    done < <(selected_rows)
    stop_qore_server
fi

if [ "$DIRECTION" = "client" ] || [ "$DIRECTION" = "both" ]; then
    while IFS=: read -r proto sdk era; do
        run_client_direction "$proto" "$era" || OVERALL_EXIT=1
    done < <(selected_rows)
fi

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
