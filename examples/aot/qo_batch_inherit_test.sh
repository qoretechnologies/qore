#!/bin/bash
# Regression harness for the AOT batch phase-sync fixes
# (commits bfe4e3e2e + 129a15d02).
#
# Exercises:
#   - Cross-session ctor-args delegation via explicit BCA
#     (Top → Mid → Base, all with `reference<list<string>>` args).
#   - Cross-session inherited member init (`Counter counter()`
#     default on Base, used by Base::constructor, inherited 2
#     levels up to Top's member_init_list).
#
# Positive test: batch-mode register in REVERSE dep order +
# MultiDeserializer's interleaved phases → batch_test() passes
# both assertions.
#
# Negative control: reverse-order register WITHOUT the batch
# wrapper (single-session addBlob per call) → batch_test() fails
# with a cross-session invariant violation (proving the batch
# phase-sync is load-bearing, not a no-op).
#
# Run from the qore repo root:
#   LD_LIBRARY_PATH=build ./examples/aot/qo_batch_inherit_test.sh

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

DEMO_DIR="examples/aot/batch_inherit_demo"
QCC="./build/qcc"

source_id() {
    realpath "$1" | sed 's/[^A-Za-z0-9_]/_/g'
}

BASE_SRC="${DEMO_DIR}/Base.qc"
MID_SRC="${DEMO_DIR}/Mid.qc"
TOP_SRC="${DEMO_DIR}/Top.qc"
MAIN_SRC="${DEMO_DIR}/main.q"
BASE_ID="$(source_id "${BASE_SRC}")"
MID_ID="$(source_id "${MID_SRC}")"
TOP_ID="$(source_id "${TOP_SRC}")"
MAIN_ID="$(source_id "${MAIN_SRC}")"
BASE_QO="${TMP}/${BASE_ID}.qo"
MID_QO="${TMP}/${MID_ID}.qo"
TOP_QO="${TMP}/${TOP_ID}.qo"
MAIN_QO="${TMP}/${MAIN_ID}.qo"

echo "=== Step 1: batch-compile 4 .qos in one parse cycle ==="
"${QCC}" -c --output-dir="${TMP}" \
    "${BASE_SRC}" \
    "${MID_SRC}" \
    "${TOP_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 2: verify per-file register entry points exist ==="
nm "${BASE_QO}" | grep -E "T qore_${BASE_ID}_${BASE_ID}_script_register" | head -1
nm "${MID_QO}"  | grep -E "T qore_${MID_ID}_${MID_ID}_script_register"   | head -1
nm "${TOP_QO}"  | grep -E "T qore_${TOP_ID}_${TOP_ID}_script_register"   | head -1
nm "${MAIN_QO}" | grep -E "T qore_${MAIN_ID}_${MAIN_ID}_script_register" | head -1

echo ""
echo "=== Step 3: build + run positive harness (batch, reverse order) ==="
cat > "${TMP}/pos.cpp" <<EOF
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>
extern "C" void qore_${BASE_ID}_${BASE_ID}_script_register(QoreProgram*);
extern "C" void qore_${MID_ID}_${MID_ID}_script_register(QoreProgram*);
extern "C" void qore_${TOP_ID}_${TOP_ID}_script_register(QoreProgram*);
extern "C" void qore_${MAIN_ID}_${MAIN_ID}_script_register(QoreProgram*);
int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    qore_aot_script_begin_batch(pgm);
    qore_${MAIN_ID}_${MAIN_ID}_script_register(pgm);
    qore_${TOP_ID}_${TOP_ID}_script_register(pgm);
    qore_${MID_ID}_${MID_ID}_script_register(pgm);
    qore_${BASE_ID}_${BASE_ID}_script_register(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "qore_aot_script_end_batch rc=%d\n", flush_rc);
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }
    int rc = qore_run_callable(pgm, "batch_test", nullptr);
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
EOF
g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/pos.cpp" "${BASE_QO}" "${MID_QO}" "${TOP_QO}" "${MAIN_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/qo_batch_inherit_test"
LD_LIBRARY_PATH=build "${TMP}/qo_batch_inherit_test"

echo ""
echo "=== Step 4: negative control (reverse order, NO batch) ==="
# Without the begin_batch wrapper, each qore_aot_script_register
# call takes the single-blob fast path — phase 1 + phase 2 run
# in one shot per blob, so when Top's register runs, Mid's class
# shell exists in pgm but Mid's own phase-1 hasn't populated its
# scl yet.  resolveClassBases should fail at
# `findClass("::BatchInherit::Mid")` since Mid's shell was only
# created this register call forward (or never, if registration
# order is wrong).  Confirm that we see the expected
# "cannot resolve base class" error on stderr.
cat > "${TMP}/neg.cpp" <<EOF
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
extern "C" void qore_${BASE_ID}_${BASE_ID}_script_register(QoreProgram*);
extern "C" void qore_${MID_ID}_${MID_ID}_script_register(QoreProgram*);
extern "C" void qore_${TOP_ID}_${TOP_ID}_script_register(QoreProgram*);
extern "C" void qore_${MAIN_ID}_${MAIN_ID}_script_register(QoreProgram*);
int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    qore_${MAIN_ID}_${MAIN_ID}_script_register(pgm);
    qore_${TOP_ID}_${TOP_ID}_script_register(pgm);
    qore_${MID_ID}_${MID_ID}_script_register(pgm);
    qore_${BASE_ID}_${BASE_ID}_script_register(pgm);
    qore_destroy_program(pgm);
    qore_cleanup();
    return 0;
}
EOF
g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/neg.cpp" "${BASE_QO}" "${MID_QO}" "${TOP_QO}" "${MAIN_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/neg"
neg_stderr=$(LD_LIBRARY_PATH=build "${TMP}/neg" 2>&1 >/dev/null)
if echo "${neg_stderr}" | grep -q "cannot resolve base class"; then
    echo "  confirmed: reverse-order registration hit 'cannot resolve base class' WITHOUT batch"
else
    echo "FAIL: expected 'cannot resolve base class' on stderr, got:"
    echo "${neg_stderr}"
    exit 1
fi

echo ""
echo "OK: AOT batch phase-sync regression test passed"
echo "    (4 .qos registered in reverse dep order; ctor-args +"
echo "    inherited member init both survived the cross-session"
echo "    interleave; negative control confirms batch is required)."
