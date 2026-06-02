#!/bin/sh
#
# Wrapper script that selects between Debug and Release Qore binaries
# based on the test being run. Performance-sensitive tests use Release mode.
#
# WebSocketH2PerfTest is too slow in Debug mode, so it always runs with Release.

# Check if WebSocketH2PerfTest is mentioned in any argument
use_release=false
for arg in "$@"; do
    if echo "$arg" | grep -q "WebSocketH2PerfTest"; then
        use_release=true
        break
    fi
done

# Find QORE_SRC_DIR if not set
if [ -z "$QORE_SRC_DIR" ]; then
    cwd=$(pwd)
    if [ -e "$cwd/qlib/SqlUtil.qm" ] || [ -e "$cwd/lib/QoreLib.cpp" ]; then
        QORE_SRC_DIR="$cwd"
    else
        # Assume we're running from the source directory
        QORE_SRC_DIR="."
    fi
fi

# Select the appropriate binary from environment variable or search for it
if [ "$use_release" = "true" ]; then
    # Try environment variable first
    if [ -n "$QORE_RELEASE_BINARY" ] && [ -x "$QORE_RELEASE_BINARY" ]; then
        QORE="$QORE_RELEASE_BINARY"
    # Search for Release binary in expected locations
    elif [ -x "${QORE_SRC_DIR}/build-release/qore" ]; then
        QORE="${QORE_SRC_DIR}/build-release/qore"
    elif [ -x "${QORE_SRC_DIR}/build/qore" ]; then
        # Fallback: use Debug if Release not found (will be slower but at least works)
        QORE="${QORE_SRC_DIR}/build/qore"
    else
        # Last resort: use environment variable even if we couldn't verify it
        QORE="${QORE_RELEASE_BINARY}"
    fi
else
    # Try environment variable first
    if [ -n "$QORE_DEBUG_BINARY" ] && [ -x "$QORE_DEBUG_BINARY" ]; then
        QORE="$QORE_DEBUG_BINARY"
    # Search for Debug binary in expected locations
    elif [ -x "${QORE_SRC_DIR}/build/qore" ]; then
        QORE="${QORE_SRC_DIR}/build/qore"
    else
        # Last resort: use environment variable even if we couldn't verify it
        QORE="${QORE_DEBUG_BINARY}"
    fi
fi

# Verify the binary exists
if [ -z "$QORE" ] || [ ! -x "$QORE" ]; then
    echo "Error: Qore binary not found or not executable: $QORE" >&2
    echo "  use_release=$use_release" >&2
    echo "  QORE_SRC_DIR=${QORE_SRC_DIR}" >&2
    echo "  QORE_DEBUG_BINARY=${QORE_DEBUG_BINARY}" >&2
    echo "  QORE_RELEASE_BINARY=${QORE_RELEASE_BINARY}" >&2
    echo "  Expected Release binary at: ${QORE_SRC_DIR}/build-release/qore" >&2
    echo "  Expected Debug binary at: ${QORE_SRC_DIR}/build/qore" >&2
    exit 1
fi

# Debug output if QORE_SELECTOR_DEBUG is set
if [ -n "$QORE_SELECTOR_DEBUG" ]; then
    echo "Binary selector: use_release=$use_release, QORE=$QORE" >&2
fi

# Execute qore with the selected binary
exec "$QORE" "$@"
