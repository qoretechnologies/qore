#!/bin/bash
# Phase C Item 2: end-to-end harness for `qcc -o <binary> *.qo`
# link mode.  Replaces the hand-written C++ main + g++ boilerplate
# the slice-10 `qo_script_test.sh` used.
#
# Workflow:
#   1. Compile lib.qc → lib.qo (self-contained).
#   2. Compile main.q → main.qo with `-L <demo-dir>` so lib.qo's
#      declarations preload for cross-file resolution.
#   3. Run `qcc -o app -e compute lib.qo main.qo` — qcc emits a
#      C++ glue main, invokes $CXX to link, and produces a
#      standalone executable.
#   4. Run the binary; compute() returns without exception.
#
# Run from the qore repo root:
#   LD_LIBRARY_PATH=build ./examples/aot/qo_link_test.sh

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
echo "=== Step 3: qcc link-mode (no hand-written main.cpp, no g++) ==="
"${QCC}" -o "${TMP}/app" -e compute "${TMP}/lib.qo" "${TMP}/main.qo" | tail -3

echo ""
echo "=== Step 4: inspect emitted glue ==="
wc -l "${TMP}/app.main.cpp"
grep -E "qore_[a-z_]+_script_register|qore_run_callable" "${TMP}/app.main.cpp" | head -5

echo ""
echo "=== Step 5: run standalone binary ==="
LD_LIBRARY_PATH=build "${TMP}/app"

echo ""
echo "OK: qcc link-mode produced standalone binary from .qo set;"
echo "    compute() ran via the emitted begin_batch / end_batch /"
echo "    qore_run_callable glue without any host-written C++."
