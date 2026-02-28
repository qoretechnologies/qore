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

# Select the appropriate binary
if [ "$use_release" = "true" ]; then
    # Use Release binary for WebSocketH2PerfTest (performance-sensitive)
    QORE="${QORE_RELEASE_BINARY}"
else
    # Use Debug binary for other tests
    QORE="${QORE_DEBUG_BINARY}"
fi

# Verify the binary exists
if [ -z "$QORE" ] || [ ! -x "$QORE" ]; then
    echo "Error: Qore binary not found or not executable: $QORE" >&2
    echo "  use_release=$use_release" >&2
    echo "  QORE_DEBUG_BINARY=${QORE_DEBUG_BINARY}" >&2
    echo "  QORE_RELEASE_BINARY=${QORE_RELEASE_BINARY}" >&2
    exit 1
fi

# Debug output if QORE_SELECTOR_DEBUG is set
if [ -n "$QORE_SELECTOR_DEBUG" ]; then
    echo "Binary selector: use_release=$use_release, QORE=$QORE" >&2
fi

# Execute qore with the selected binary
exec "$QORE" "$@"
