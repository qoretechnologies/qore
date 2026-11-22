#!/bin/sh
# rows.sh <build> [<base build>]: the callgrind rows (cgmin.sh) of a build, or of a build and a base build with the
# delta (see README.md in this directory)
#
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Prints one line per row and mode, in the order of ROWS and MODES:
#   <script>:<row> <mode> <value>                         (one build)
#   <script>:<row> <mode> <base value> <value> <delta>    (two builds)
#
# environment (and those of cgmin.sh, which it passes on):
#   ROWS         the rows as <script>:<row> words (scripts in bench/), or "standard" (default: the standard rows) or
#                "all" (every row of README.md's list)
#   MODES        the modes (default: "ast ir jit")
#   P            the number of cells run at a time (default 4)
#   CGROW_WRAP   a command prefix for each cgmin.sh run, ex: "qbuild --count --" to run them in count slots
D=$(cd "$(dirname "$0")" && pwd) || exit 1
if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "usage: $0 <build> [<base build>]" >&2
    exit 2
fi
NEW=$1
BASE=$2
STANDARD="bench.q:fs bench-func.q:fc bench-func.q:sc bench-method.q:m bench-map.q:mp bench-map.q:mm"
ALL="$STANDARD bench-method.q:mse bench-map.q:mmse bench-pseudo.q:pv bench-pseudo.q:pl bench-pseudo.q:pt \
bench-member.q:mr bench-member.q:mw bench-member.q:pr bench-member.q:pi bench-cf.q:cf bench-cf.q:cfa"
case ${ROWS:-standard} in
    standard) ROWS=$STANDARD ;;
    all) ROWS=$ALL ;;
esac
MODES=${MODES:-ast ir jit}
P=${P:-4}
case $P in
    ''|*[!0-9]*|0) echo "$0: P must be a positive integer" >&2; exit 2 ;;
esac
for r in $ROWS; do
    case $r in
        *:*) [ -f "$D/bench/${r%%:*}" ] || { echo "$0: no bench script '${r%%:*}' in $D/bench" >&2; exit 2; } ;;
        *) echo "$0: row '$r' is not <script>:<row>" >&2; exit 2 ;;
    esac
done
O=$(mktemp -d "${TMPDIR:-/tmp}/cgrows.XXXXXX") || exit 1
trap 'rm -rf "$O"' EXIT
trap 'exit 1' HUP INT TERM

# the cells: one line "<build tag> <build> <script> <row> <mode>" each
for r in $ROWS; do
    for m in $MODES; do
        echo "new $NEW ${r%%:*} ${r##*:} $m"
        if [ -n "$BASE" ]; then
            echo "base $BASE ${r%%:*} ${r##*:} $m"
        fi
    done
done > "$O/cells"
# each cell writes its value to $O/<tag>.<script>.<row>.<mode>, or its error output to the same name with .err
# (the paths of the builds must not contain white space)
export CGROW_WRAP
# shellcheck disable=SC2016
xargs -P "$P" -L 1 sh -c 'v=$($CGROW_WRAP sh "$0/cgmin.sh" "$3" "$0/bench/$4" "$5" "$6" 2> "$1/$2.$4.$5.$6.err") \
    && echo "$v" > "$1/$2.$4.$5.$6"' "$D" "$O" < "$O/cells" || true
rc=0
for r in $ROWS; do
    s=${r%%:*}
    l=${r##*:}
    for m in $MODES; do
        for t in new base; do
            if [ "$t" = base ] && [ -z "$BASE" ]; then
                continue
            fi
            if [ ! -s "$O/$t.$s.$l.$m" ]; then
                echo "$0: the cell $r $m of the $t build failed:" >&2
                cat "$O/$t.$s.$l.$m.err" >&2
                rc=1
            fi
        done
        a=$(cat "$O/new.$s.$l.$m" 2>/dev/null)
        if [ -n "$BASE" ]; then
            b=$(cat "$O/base.$s.$l.$m" 2>/dev/null)
            d=$(awk -v a="$a" -v b="$b" 'BEGIN { if (a == "" || b == "") print "-"; else print a - b }')
            echo "$r $m ${b:--} ${a:--} $d"
        else
            echo "$r $m ${a:--}"
        fi
    done
done
exit $rc
