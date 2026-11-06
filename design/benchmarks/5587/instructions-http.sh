#!/bin/bash
# User-space instructions executed by the server process of run-http.sh (#5587), for <requests> requests.
#
# usage: design/benchmarks/5587/instructions-http.sh <keepalive 0|1> <requests>
#
# Run from the repository root.  The count does not depend on the load of the machine, unlike CPU time, so it can be
# compared between runs and builds on a shared host.  Per-request cost: (count for N requests - count for 1 request) / (N - 1).
export LD_LIBRARY_PATH=build QORE_IO_THREADS=8 QORE_MODULE_DIR=$PWD/qlib
dir=$(cd "$(dirname "$0")" && pwd)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fifo=$T/fifo
mkfifo $fifo
perf stat -x, -e instructions:u -o $T/stat build/qore $dir/http-server.qr > $fifo 2>/dev/null &
spid=$!
read -r _ < $fifo
exec 3< $fifo
build/qore $dir/http-client.qr "$1" "$2" > /dev/null 2>&1
cat <&3 > /dev/null
wait $spid
grep instructions $T/stat | cut -d, -f1
