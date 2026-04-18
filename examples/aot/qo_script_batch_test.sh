#!/bin/bash
# AOT Phase 4 slice 10g end-to-end harness for the script-context
# out-of-order register batch API.
#
# Exercises:
#   - slice 10g: `qore_aot_script_begin_batch` /
#     `qore_aot_script_end_batch` — register .qo files in REVERSE
#     of dependency order and rely on end_batch's cross-blob
#     resolution pass.
#   - slice 10i: batch compile (N .qos from one parse cycle).
#
# Positive test: reverse-order registration + batch mode -> compute()
# runs successfully.
# Negative control: same reverse-order without batch mode -> compute()
# fails (proving the batch is load-bearing, not a no-op).
#
# Run from the qore repo root:
#   LD_LIBRARY_PATH=build ./examples/aot/qo_script_batch_test.sh

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

DEMO_DIR="examples/aot/script_demo"
QCC="./build/qcc"

echo "=== Step 1: batch-compile lib.qc + main.q in one parse cycle ==="
"${QCC}" -c --output-dir="${TMP}" \
    "${DEMO_DIR}/lib.qc" "${DEMO_DIR}/main.q" | tail -4

echo ""
echo "=== Step 2: verify per-file register entry points exist ==="
nm "${TMP}/lib.qo"  | grep -E "T qore_lib_lib_script_register"   | head -1
nm "${TMP}/main.qo" | grep -E "T qore_main_main_script_register" | head -1

echo ""
echo "=== Step 3: build + run positive harness (batch, reverse order) ==="
g++ -std=c++17 -Iinclude -Lbuild \
    examples/aot/qo_script_batch_test.cpp \
    "${TMP}/lib.qo" "${TMP}/main.qo" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/qo_script_batch_test"
LD_LIBRARY_PATH=build "${TMP}/qo_script_batch_test"

echo ""
echo "=== Step 4: negative control (reverse order, NO batch) ==="
cat > "${TMP}/neg.cpp" <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>
extern "C" void qore_lib_lib_script_register(QoreProgram*);
extern "C" void qore_main_main_script_register(QoreProgram*);
int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    qore_main_main_script_register(pgm);   // reversed — no batch
    qore_lib_lib_script_register(pgm);
    int rc = qore_run_callable(pgm, "compute", nullptr);
    qore_destroy_program(pgm);
    qore_cleanup();
    // We want compute() to FAIL without batch; invert the exit code.
    return rc == 0 ? 2 : 0;
}
EOF
g++ -std=c++17 -Iinclude -Lbuild \
    "${TMP}/neg.cpp" "${TMP}/lib.qo" "${TMP}/main.qo" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/neg"
# Suppress the expected error spew.
if LD_LIBRARY_PATH=build "${TMP}/neg" >/dev/null 2>&1; then
    echo "  confirmed: reverse-order registration failed WITHOUT batch"
else
    echo "FAIL: reverse-order-without-batch unexpectedly succeeded"
    exit 1
fi

echo ""
echo "OK: slice 10g out-of-order batch register works end-to-end"
echo "    (batch-compiled 2 .qos, registered in reverse dep order,"
echo "    compute() ran; negative control confirms batch is required)."
