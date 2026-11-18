#!/bin/bash
# Server CPU per request of an HttpServer with a trivial handler (#5587).
#
# usage: design/benchmarks/5587/run-http.sh <keepalive 0|1> <requests>
#
# Run from the repository root with a release build in ./build (and the HttpServer, HttpServerAsyncIo and
# HttpServerUtil -qmod targets built, so the modules run AOT-compiled as they do when installed).
# The server runs in its own process and reports its own CPU time when the client asks it to quit, so the client's
# cost is not counted.  The server prints "ready <port>" once it listens on a port of its own; the script waits for that
# line, not for a time, and gives the port to the client, so that concurrent runs do not share a server.  With
# HTTP_BENCH_PORT=<port>, the server listens on that port (for loadgen.c).
set -e
dir=$(cd "$(dirname "$0")" && pwd)
# the binary modules of the build come first, so that the AOT modules in qlib (built against them) are not stale
export LD_LIBRARY_PATH=build QORE_IO_THREADS=${QORE_IO_THREADS:-8} QORE_MODULE_DIR=$(ls -d $PWD/build/modules/*/ 2>/dev/null | tr '\n' ':')$PWD/qlib
T=$(mktemp -d)
spid=
stop_server() {
    if [ -n "$spid" ]; then
        kill $spid 2>/dev/null || true
        wait $spid 2>/dev/null || true
        spid=
    fi
}
trap 'stop_server; rm -rf "$T"' EXIT
fifo=$T/fifo
mkfifo "$fifo"
build/qore "$dir/http-server.qr" > "$fifo" &
spid=$!
# the fifo is opened once, so that a server that fails to start ends the script instead of leaving it waiting
exec 3< "$fifo"
if ! read -r _ port <&3 || [ -z "$port" ]; then
    echo "the server did not start" >&2
    exit 1
fi
echo "server port: $port" >&2
if ! build/qore "$dir/http-client.qr" "$1" "$2" 1 "$port"; then
    # the client failed; the server may not have been asked to quit
    exit 1
fi
cat <&3
wait $spid
spid=
