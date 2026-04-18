#!/bin/bash
# AOT Phase 4 slice 10 end-to-end harness for script-context AOT.
#
# Exercises:
#   - slice 10c: `qcc -c -L<dir>` cross-.qo preload for a plain
#     multi-file Qore app (no .qm, no %module).
#   - slice 10d: `qore_<sanapp>_<sanfile>_script_register` per-file
#     entry points + the runtime `qore_aot_script_register` API.
#   - slice 8: `qore_create_program` / `qore_run_callable` /
#     `qore_destroy_program` C API from a C++ host.
#
# Workflow:
#   1. Compile lib.qc → lib.qo (self-contained).
#   2. Compile main.q → main.qo with `-L <demo-dir>` so lib.qo's
#      declarations preload for cross-file resolution.
#   3. Build qo_script_test.cpp as a C++ host linked against both
#      .qo's + -lqore.
#   4. Run the host; compute() returns without exception.
#
# Run from the qore repo root:
#   LD_LIBRARY_PATH=build ./examples/aot/qo_script_test.sh

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

DEMO_DIR="examples/aot/script_demo"
QCC="./build/qcc"

echo "=== Step 1: compile lib.qc ==="
"${QCC}" -c -o "${TMP}/lib.qo" "${DEMO_DIR}/lib.qc" | tail -2

echo ""
echo "=== Step 2: compile main.q with -L preload ==="
"${QCC}" -c -L "${TMP}" -o "${TMP}/main.qo" "${DEMO_DIR}/main.q" | tail -2

echo ""
echo "=== Step 3: inspect script register symbols ==="
echo "--- lib.qo ---"
nm "${TMP}/lib.qo" | grep -E "T (Helper::|qore_lib_lib_script_register)" || true
echo "--- main.qo ---"
nm "${TMP}/main.qo" | grep -E "T (compute\\(\\)|qore_main_main_script_register)" || true

echo ""
echo "=== Step 4: ld -r cleanliness ==="
ld -r "${TMP}/lib.qo" "${TMP}/main.qo" -o "${TMP}/combined.o"
DUPS=$(nm "${TMP}/combined.o" | awk '$2 == "T" {print $3}' | sort | uniq -d | wc -l)
DEFS=$(nm "${TMP}/combined.o" | awk '$2 == "T"' | wc -l)
echo "  duplicate T symbols: ${DUPS} (expect 0)"
echo "  defined T symbols:   ${DEFS}"
[ "${DUPS}" = "0" ] || { echo "FAIL: duplicate symbols"; exit 1; }

echo ""
echo "=== Step 5: link C++ host + run ==="
g++ -std=c++17 -Iinclude -Ibuild -Ibuild/include -Lbuild \
    examples/aot/qo_script_test.cpp \
    "${TMP}/lib.qo" "${TMP}/main.qo" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/qo_script_test"
LD_LIBRARY_PATH=build "${TMP}/qo_script_test"

echo ""
echo "OK: script-context AOT end-to-end (lib.qc + main.q compiled per-file,"
echo "    cross-.qo class ref resolved, compute() ran through static linkage)."
