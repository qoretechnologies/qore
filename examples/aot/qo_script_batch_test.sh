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
# runs successfully.  Late-bound non-hierarchy references in this fixture
# also work without batch after both fragments are registered; class hierarchy
# cases where batch remains load-bearing are covered by
# qo_batch_inherit_test.sh.
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

source_id() {
    realpath "$1" | sed 's/[^A-Za-z0-9_]/_/g'
}

LIB_SRC="${DEMO_DIR}/lib.qc"
MAIN_SRC="${DEMO_DIR}/main.q"
LIB_ID="$(source_id "${LIB_SRC}")"
MAIN_ID="$(source_id "${MAIN_SRC}")"
LIB_QO="${TMP}/${LIB_ID}.qo"
MAIN_QO="${TMP}/${MAIN_ID}.qo"

echo "=== Step 1: batch-compile lib.qc + main.q in one parse cycle ==="
"${QCC}" -c --output-dir="${TMP}" \
    "${LIB_SRC}" "${MAIN_SRC}" | tail -4

echo ""
echo "=== Step 2: verify per-file register entry points exist ==="
nm "${LIB_QO}"  | grep -E "T qore_${LIB_ID}_${LIB_ID}_script_register"   | head -1
nm "${MAIN_QO}" | grep -E "T qore_${MAIN_ID}_${MAIN_ID}_script_register" | head -1

echo ""
echo "=== Step 3: build + run positive harness (batch, reverse order) ==="
cat > "${TMP}/pos.cpp" <<EOF
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>
extern "C" void qore_${LIB_ID}_${LIB_ID}_script_register(QoreProgram*);
extern "C" void qore_${MAIN_ID}_${MAIN_ID}_script_register(QoreProgram*);
int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    qore_aot_script_begin_batch(pgm);
    qore_${MAIN_ID}_${MAIN_ID}_script_register(pgm);
    qore_${LIB_ID}_${LIB_ID}_script_register(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "batch flush failed: %s\n", qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }
    int rc = qore_run_callable(pgm, "compute", nullptr);
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
EOF
g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/pos.cpp" "${LIB_QO}" "${MAIN_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/qo_script_batch_test"
LD_LIBRARY_PATH=build "${TMP}/qo_script_batch_test"

echo ""
echo "=== Step 4: compatibility control (reverse order, NO batch) ==="
cat > "${TMP}/compat.cpp" <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>
EOF
cat >> "${TMP}/compat.cpp" <<EOF
extern "C" void qore_${LIB_ID}_${LIB_ID}_script_register(QoreProgram*);
extern "C" void qore_${MAIN_ID}_${MAIN_ID}_script_register(QoreProgram*);
EOF
cat >> "${TMP}/compat.cpp" <<EOF
int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    qore_${MAIN_ID}_${MAIN_ID}_script_register(pgm);   // reversed - no batch
    qore_${LIB_ID}_${LIB_ID}_script_register(pgm);
    int rc = qore_run_callable(pgm, "compute", nullptr);
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
EOF
g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/compat.cpp" "${LIB_QO}" "${MAIN_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/compat"
LD_LIBRARY_PATH=build "${TMP}/compat"
echo "  confirmed: reverse-order registration without batch also works for this late-bound fixture"

echo ""
echo "OK: slice 10g out-of-order batch register works end-to-end"
echo "    (batch-compiled 2 .qos, registered in reverse dep order,"
echo "    compute() ran; no-batch compatibility control also ran)."
