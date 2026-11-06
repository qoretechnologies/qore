#!/bin/bash
# Server CPU per request of an HttpServer with a trivial handler (#5587).
#
# usage: design/benchmarks/5587/run-http.sh <keepalive 0|1> <requests>
#
# Run from the repository root with a release build in ./build (and the HttpServer, HttpServerAsyncIo and
# HttpServerUtil -qmod targets built, so the modules run AOT-compiled as they do when installed).
# The server runs in its own process and reports its own CPU time when the client asks it to quit, so the client's
# cost is not counted.  The server prints "ready" once it listens; the script waits for that line, not for a time.
set -e
dir=$(cd "$(dirname "$0")" && pwd)
export LD_LIBRARY_PATH=build QORE_IO_THREADS=${QORE_IO_THREADS:-8} QORE_MODULE_DIR=$PWD/qlib
fifo=$(mktemp -u)
mkfifo "$fifo"
trap 'rm -f "$fifo"' EXIT
build/qore "$dir/http-server.qr" > "$fifo" &
spid=$!
read -r _ < "$fifo"
exec 3< "$fifo"
build/qore "$dir/http-client.qr" "$1" "$2" || true
cat <&3
wait $spid
