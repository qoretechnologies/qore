#!/bin/sh
# cgmin.sh <build> <script> <row> <mode>: callgrind instructions per iteration of one loop ("row") of a bench script,
# the minimum over allocation layouts (qore#7193; see README.md in this directory)
#
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# <build>   a build directory with qore (and qcc for the aot modes) and libqore
# <script>  a bench script (bench/*.q): its first line is %modern, its loops start with "int t = 0;", it defines
#           "int sub f(int a)", and it runs the loop that L names N times
# <row>     the loop to run (the script's L value)
# <mode>    ast, ir, jit or tiered (qore --exec-mode), or aot0..aot3 (an executable built by the build's qcc -O<n>)
#
# For each layout k (0 .. CGROW_LAYOUTS - 1) the script is rewritten as:
# - a pad: after the first line, before anything else of the script is parsed, a constant string of k * CGROW_PAD_STEP
#   characters (the pad string itself has CGROW_PAD_STEP characters more in each layout, so every later heap
#   allocation, the member names of the classes declared first among them, moves);
# - k dead statements ("if (False) { f(<i>); }") after each "int t = 0;", which change what the parse allocates
#   between the objects of the loop's function;
# and the instructions of a run with N=CGROW_N1 less those of a run with N=CGROW_N0, over CGROW_N1 - CGROW_N0, give its
# value; the minimum over the layouts is printed.
#
# environment:
#   CGROW_LAYOUTS    the number of layouts (default 5)
#   CGROW_PAD        1 (default): the pad moves with the layout; 0: no pad (the layouts of the old cgmin2.sh)
#   CGROW_PAD_STEP   the pad's growth per layout in characters (default 816)
#   CGROW_N0         the iterations of the first run (default 2000)
#   CGROW_N1         the iterations of the second run (default 22000)
#   CGROW_DECIMALS   0 (default): the value as an integer (truncated, as cgmin2.sh printed it); n > 0: n decimals
#   CGROW_VERBOSE    1: also print "layout <k> <value>" for each layout on stderr
#   CGROW_VALGRIND   the valgrind command (default: valgrind)
#   CGROW_KEEP       a directory to copy each layout's script to (layout-<k>.q), to inspect them
B=$1
S=$2
L=$3
M=$4
if [ $# -ne 4 ]; then
    echo "usage: $0 <build> <script> <row> <mode>" >&2
    exit 2
fi
LAYOUTS=${CGROW_LAYOUTS:-5}
PAD=${CGROW_PAD:-1}
STEP=${CGROW_PAD_STEP:-816}
N0=${CGROW_N0:-2000}
N1=${CGROW_N1:-22000}
DEC=${CGROW_DECIMALS:-0}
VG=${CGROW_VALGRIND:-valgrind}
case $M in
    ast|ir|jit|tiered) ;;
    aot[0-3]) ;;
    *) echo "$0: unknown mode '$M' (ast, ir, jit, tiered, aot0..aot3)" >&2; exit 2 ;;
esac
for v in "$LAYOUTS" "$STEP" "$N0" "$N1" "$DEC"; do
    case $v in
        ''|*[!0-9]*) echo "$0: CGROW_LAYOUTS, CGROW_PAD_STEP, CGROW_N0, CGROW_N1 and CGROW_DECIMALS must be" \
            "non-negative integers" >&2; exit 2 ;;
    esac
done
if [ "$LAYOUTS" -lt 1 ] || [ "$N1" -le "$N0" ]; then
    echo "$0: CGROW_LAYOUTS must be at least 1 and CGROW_N1 greater than CGROW_N0" >&2
    exit 2
fi
if [ ! -f "$S" ]; then
    echo "$0: no script '$S'" >&2
    exit 2
fi
if [ "$(sed -n 1p "$S")" != "%modern" ]; then
    echo "$0: the first line of '$S' is not %modern" >&2
    exit 2
fi
O=$(mktemp -d "${TMPDIR:-/tmp}/cgmin.XXXXXX") || exit 1
trap 'rm -rf "$O"' EXIT
trap 'exit 1' HUP INT TERM

# run <n>: the instructions of a run of the layout's script (or executable) with N=<n>
run() {
    case $M in
        aot*) L=$L N=$1 LD_LIBRARY_PATH=$B $VG --tool=callgrind --fair-sched=yes \
                --callgrind-out-file="$O/cg.out" "$O/exe" -b > /dev/null 2> "$O/cg.log" ;;
        *) L=$L N=$1 QORE_JIT_SYNC_COMPILE=1 LD_LIBRARY_PATH=$B $VG --tool=callgrind --fair-sched=yes \
                --callgrind-out-file="$O/cg.out" "$B/qore" -b --exec-mode=$M "$O/bench.q" > /dev/null 2> "$O/cg.log" ;;
    esac
    rc=$?
    c=$(sed -n 's/.*Collected : \([0-9][0-9]*\).*/\1/p' "$O/cg.log")
    if [ $rc -ne 0 ] || [ -z "$c" ]; then
        echo "$0: the run with N=$1 of layout $k failed (exit code $rc):" >&2
        cat "$O/cg.log" >&2
        return 1
    fi
    rm -f "$O/cg.out" "$O/cg.log"
    echo "$c"
}

min=
k=0
while [ $k -lt "$LAYOUTS" ]; do
    dead=""
    i=0
    while [ $i -lt $k ]; do
        dead="$dead if (False) { f($i); }"
        i=$((i + 1))
    done
    {
        sed -n 1p "$S"
        if [ "$PAD" = 1 ]; then
            # the pad: a string of k * STEP characters, parsed before anything else of the script
            printf 'const CGROW_PAD = "%s";\n' "$(awk -v n=$((k * STEP)) 'BEGIN { while (n-- > 0) printf "p" }')"
        fi
        sed "1d;s/int t = 0;/int t = 0;$dead/" "$S"
    } > "$O/bench.q"
    if [ -n "$CGROW_KEEP" ]; then
        mkdir -p "$CGROW_KEEP" && cp "$O/bench.q" "$CGROW_KEEP/layout-$k.q" || exit 1
    fi
    case $M in
        aot*) if ! LD_LIBRARY_PATH=$B "$B/qcc" -O"${M#aot}" -o "$O/exe" "$O/bench.q" > "$O/qcc.log" 2>&1; then
                  cat "$O/qcc.log" >&2
                  exit 1
              fi ;;
    esac
    a=$(run "$N0") || exit 1
    b=$(run "$N1") || exit 1
    v=$(awk -v a="$a" -v b="$b" -v d="$((N1 - N0))" -v p="$DEC" 'BEGIN {
        if (p == 0) { printf "%d", int((b - a) / d) } else { printf "%." p "f", (b - a) / d } }')
    if [ "$CGROW_VERBOSE" = 1 ]; then
        echo "layout $k $v" >&2
    fi
    if [ -z "$min" ] || awk -v v="$v" -v m="$min" 'BEGIN { exit !(v + 0 < m + 0) }'; then
        min=$v
    fi
    k=$((k + 1))
done
echo "$min"
