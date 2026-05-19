#!/bin/bash
# Compare generic-heavy kernels across interpreter modes and AOT source modes.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
CASE="$HERE/generic-aot-specialization.qr"
QORE_BIN="${QORE_BIN:-$REPO/build/qore}"
QCC_BIN="${QCC_BIN:-$REPO/build/qcc}"
OUTDIR="${OUTDIR:-$REPO/build/bench-generic-aot-specialization}"

SAMPLES="${SAMPLES:-7}"
WARMUP="${WARMUP:-2}"
INNER="${INNER:-400000}"
HASH_INNER="${HASH_INNER:-50000}"

usage() {
    cat <<EOF
usage: $0 [benchmark-options]

Environment:
  QORE_BIN=PATH       qore executable (default: build/qore)
  QCC_BIN=PATH        qcc executable (default: build/qcc)
  OUTDIR=DIR          AOT executable output dir
  SAMPLES=N           timed samples per kernel (default: 7)
  WARMUP=N            warmup samples per kernel (default: 2)
  INNER=N             inner loop count for call kernels (default: 400000)
  HASH_INNER=N        inner loop count for hashdecl kernel (default: 50000)

Any benchmark option accepted by generic-aot-specialization.qr can be passed
after these defaults; later duplicate options win.
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

if [[ ! -x "$QORE_BIN" ]]; then
    echo "ERROR: qore binary not found/executable at $QORE_BIN" >&2
    exit 2
fi
if [[ ! -x "$QCC_BIN" ]]; then
    echo "ERROR: qcc binary not found/executable at $QCC_BIN" >&2
    exit 2
fi

mkdir -p "$OUTDIR"

AOT_WITH_SOURCE="$OUTDIR/generic-aot-include-source"
AOT_STRIPPED="$OUTDIR/generic-aot-strip-source"

echo "# qore: $QORE_BIN" >&2
echo "# qcc:  $QCC_BIN" >&2
echo "# case: $CASE" >&2
echo "# compiling AOT benchmark executables" >&2
"$QCC_BIN" --include-source -o "$AOT_WITH_SOURCE" "$CASE" >/dev/null
"$QCC_BIN" --strip-source -o "$AOT_STRIPPED" "$CASE" >/dev/null

echo "# aot_include_source_bytes=$(stat -c '%s' "$AOT_WITH_SOURCE")" >&2
echo "# aot_strip_source_bytes=$(stat -c '%s' "$AOT_STRIPPED")" >&2

COMMON=("--samples=$SAMPLES" "--warmup=$WARMUP" "--inner=$INNER" "--hash-inner=$HASH_INNER" "--no-header")

printf "%-20s %-28s %10s %10s %10s %10s %10s %10s %18s\n" \
    "mode" "kernel" "inner" "median_ms" "mean_ms" "min_ms" "max_ms" "stdev_ms" "checksum"
printf "%s\n" "----------------------------------------------------------------------------------------------------------------------------------"

"$QORE_BIN" --exec-mode=ast "$CASE" --mode=ast "${COMMON[@]}" "$@"
"$QORE_BIN" --exec-mode=ir "$CASE" --mode=ir "${COMMON[@]}" "$@"
"$QORE_BIN" --exec-mode=jit "$CASE" --mode=jit "${COMMON[@]}" "$@"
"$QORE_BIN" --exec-mode=tiered "$CASE" --mode=tiered "${COMMON[@]}" "$@"
"$AOT_WITH_SOURCE" --mode=aot_include_source "${COMMON[@]}" "$@"
"$AOT_STRIPPED" --mode=aot_strip_source "${COMMON[@]}" "$@"
