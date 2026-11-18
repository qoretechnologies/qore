#!/bin/bash
# User-space instructions executed by the server process of run-http.sh (#5587), for <requests> requests.
#
# usage: design/benchmarks/5587/instructions-http.sh <keepalive 0|1> <requests>
#
# Run from the repository root.  The count does not depend on the load of the machine, unlike CPU time, so it can be
# compared between runs and builds on a shared host.  Per-request cost: (count for N requests - count for 1 request) / (N - 1).
# the binary modules of the build come first, so that the AOT modules in qlib (built against them) are not stale
export LD_LIBRARY_PATH=build QORE_IO_THREADS=8 QORE_MODULE_DIR=$(ls -d $PWD/build/modules/*/ 2>/dev/null | tr '\n' ':')$PWD/qlib
dir=$(cd "$(dirname "$0")" && pwd)
T=$(mktemp -d) || exit 1
spid=
# the server runs under perf stat, which does not forward signals: the server itself is stopped, and perf stat ends
# with it
stop_server() {
    if [ -n "$spid" ]; then
        pkill -TERM -P $spid 2>/dev/null
        wait $spid 2>/dev/null
        spid=
    fi
}
trap 'stop_server; rm -rf "$T"' EXIT
fifo=$T/fifo
mkfifo "$fifo" || exit 1
perf stat -x, -e instructions:u -o $T/stat build/qore $dir/http-server.qr > "$fifo" 2>/dev/null &
spid=$!
# the server listens on a port of its own and prints it in its "ready <port>" line; the fifo is opened once, so that
# a server that fails to start ends the script (end of file) instead of leaving it waiting for a writer
exec 3< "$fifo"
if ! read -r _ port <&3 || [ -z "$port" ]; then
    echo "the server did not start" >&2
    exit 1
fi
if ! build/qore $dir/http-client.qr "$1" "$2" 1 "$port" > /dev/null 2>$T/client.err; then
    # the client failed; the server may not have been asked to quit
    cat $T/client.err >&2
    exit 1
fi
cat <&3 > /dev/null
wait $spid
spid=
grep instructions $T/stat | cut -d, -f1
