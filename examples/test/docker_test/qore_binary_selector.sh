#!/bin/sh
#
# Wrapper script that selects between Debug and Release Qore binaries
# based on the test being run. Performance-sensitive tests use Release mode.
#
# WebSocketH2PerfTest is too slow in Debug mode, so it always runs with Release.

# Find the test file argument (should be the last .qtest file mentioned)
test_file=""
for arg in "$@"; do
    if [ -f "$arg" ] && echo "$arg" | grep -q "\.qtest$"; then
        test_file="$arg"
    fi
done

# Default to Debug if we can't find the test file (shouldn't happen)
if [ -z "$test_file" ]; then
    QORE="${QORE_DEBUG_BINARY:-./build/qore}"
elif echo "$test_file" | grep -q "WebSocketH2PerfTest"; then
    # Use Release binary for WebSocketH2PerfTest (performance-sensitive)
    QORE="${QORE_RELEASE_BINARY:-./build-release/qore}"
else
    # Use Debug binary for other tests
    QORE="${QORE_DEBUG_BINARY:-./build/qore}"
fi

# Execute qore with the selected binary
exec "$QORE" "$@"
