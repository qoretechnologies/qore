#!/bin/bash
# bench/run.sh - orchestrate bench/cases/bench_*.qr, aggregate results.

set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"

QORE_BIN="${QORE_BIN:-$REPO/build/qore}"
MODULE_PATHS=("$HERE/lib")
for path in "$REPO/build/modules/dataframe" "$REPO/build/qlib-qmod" "$REPO/qlib"; do
    if [[ -d "$path" ]]; then
        MODULE_PATHS+=("$path")
    fi
done
QORE_MODULE_DIR="$(IFS=:; echo "${MODULE_PATHS[*]}")${QORE_MODULE_DIR:+:$QORE_MODULE_DIR}"
export QORE_MODULE_DIR

ITERATIONS=""
WARMUP=""
SAVE=""
BASELINE=""
FILTER=""
QUIET_FLAG=""

usage() {
    cat <<EOF
usage: $0 [options]
  --iterations=N    override per-case iteration count
  --warmup=N        override per-case warmup count
  --save=FILE       write aggregated suite JSON to FILE
  --baseline=FILE   compare current run against baseline suite JSON
  --filter=REGEX    only run cases whose name matches REGEX
  --quiet           suppress per-case human report (JSON save still works)

environment:
  QORE_BIN                 qore executable to benchmark (default: build/qore)
  QORE_BENCH_DF_ROWS       DataFrame in-memory benchmark row count (default: 100000)
  QORE_BENCH_DF_SQL_ROWS   DataFrame SQL benchmark row count (default: 20000)
  QORE_BENCH_DF_SOLUTION_ROWS
                           End-to-end ETL solution row count (default: QORE_BENCH_DF_ROWS)
  QORE_BENCH_DF_AI_ROWS    End-to-end AI/analytics solution row count (default: 20000)
  QORE_DB_CONNSTR_PGSQL    enables optional PostgreSQL DataFrame SQL cases
EOF
    exit 1
}

for arg in "$@"; do
    case "$arg" in
        --iterations=*) ITERATIONS="$arg" ;;
        --warmup=*)     WARMUP="$arg" ;;
        --save=*)       SAVE="${arg#--save=}" ;;
        --baseline=*)   BASELINE="${arg#--baseline=}" ;;
        --filter=*)     FILTER="${arg#--filter=}" ;;
        --quiet)        QUIET_FLAG="--quiet" ;;
        -h|--help)      usage ;;
        *) echo "unknown arg: $arg" >&2; usage ;;
    esac
done

if [[ ! -x "$QORE_BIN" ]]; then
    echo "ERROR: qore binary not found/executable at $QORE_BIN" >&2
    echo "       set QORE_BIN or build with: cmake --build build" >&2
    exit 2
fi

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

echo "qore:   $QORE_BIN"
echo "commit: $(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo unknown)"
echo

for case_file in "$HERE"/cases/bench_*.qr; do
    name=$(basename "$case_file" .qr)
    name="${name#bench_}"
    if [[ -n "$FILTER" && ! "$name" =~ $FILTER ]]; then
        continue
    fi
    out="$TMPDIR/$name.json"
    "$QORE_BIN" "$case_file" \
        ${ITERATIONS:+$ITERATIONS} \
        ${WARMUP:+$WARMUP} \
        --save="$out" \
        $QUIET_FLAG \
        || { echo "FAILED: $name" >&2; }
done

# Aggregate all per-case JSON into one suite JSON.
AGG="$HERE/_aggregate.qr"
"$QORE_BIN" "$AGG" "$TMPDIR" ${SAVE:+--save="$SAVE"} ${BASELINE:+--baseline="$BASELINE"}
