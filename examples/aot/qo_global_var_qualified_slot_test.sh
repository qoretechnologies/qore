#!/bin/bash
# Regression: source-stripped aggregate AOT global slots must preserve
# namespace-qualified global variable identity.  A simple-name slot for
# `omqp` can bind to a different namespace's `omqp` when duplicate names exist.

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

QCC="${QCC:-./build/qcc}"
mkdir -p "${TMP}/src" "${TMP}/qo"

source_id() {
    realpath "$1" | sed 's/[^A-Za-z0-9_]/_/g'
}

GLOBALS_SRC="${TMP}/src/globals.qc"
MANAGER_SRC="${TMP}/src/manager.qc"
MAIN_SRC="${TMP}/src/main.q"
AGG_QO="${TMP}/qo/global_var_qualified_slot_agg.qo"

cat >"${GLOBALS_SRC}" <<'QORE'
%new-style
%strict-args
%require-types
%require-our

public class PoolHandle {
    public {
        string name;
    }

    constructor(string n) {
        name = n;
    }
}

our PoolHandle omqp;

public namespace RbacModule {
    public our PoolHandle omqp;
}
QORE

cat >"${MANAGER_SRC}" <<'QORE'
%new-style
%strict-args
%require-types
%require-our

public class Manager {
    public PoolHandle getPool() {
        return omqp;
    }
}
QORE

cat >"${MAIN_SRC}" <<'QORE'
%new-style
%strict-args
%require-types
%require-our

int sub global_var_qualified_slot_test() {
    omqp = new PoolHandle("root");
    RbacModule::omqp = new PoolHandle("rbac");

    Manager mgr();
    PoolHandle dsp = mgr.getPool();
    if (dsp.name != "root") {
        throw "AOT-GLOBAL-SLOT-ERROR", sprintf("unexpected global value: %y", dsp.name);
    }
    return 0;
}
QORE

GLOBALS_ID="$(source_id "${GLOBALS_SRC}")"
MANAGER_ID="$(source_id "${MANAGER_SRC}")"
MAIN_ID="$(source_id "${MAIN_SRC}")"
GLOBALS_QO="${TMP}/qo/${GLOBALS_ID}.qo"
MANAGER_QO="${TMP}/qo/${MANAGER_ID}.qo"
MAIN_QO="${TMP}/qo/${MAIN_ID}.qo"

echo "=== Step 1: batch-compile per-file script objects ==="
"${QCC}" -c --output-dir="${TMP}/qo" \
    "${GLOBALS_SRC}" \
    "${MANAGER_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 2: compile native-registering script aggregate ==="
"${QCC}" -c \
    -o "${AGG_QO}" \
    --script-aggregate=global_var_qualified_slot_agg \
    --script-aggregate-native-registers \
    "${GLOBALS_SRC}" \
    "${MANAGER_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 3: build and run aggregate registration harness ==="
cat >"${TMP}/harness.cpp" <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>

extern "C" void init_global_var_qualified_slot_agg_qo(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES | PO_REQUIRE_OUR);
    qore_aot_script_begin_batch(pgm);
    init_global_var_qualified_slot_agg_qo(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "qore_aot_script_end_batch rc=%d: %s\n", flush_rc, qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }
    int rc = qore_run_callable(pgm, "global_var_qualified_slot_test", nullptr);
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
EOF

g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/harness.cpp" \
    "${AGG_QO}" \
    "${GLOBALS_QO}" \
    "${MANAGER_QO}" \
    "${MAIN_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/global_var_qualified_slot_test"

LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${TMP}/global_var_qualified_slot_test"

echo ""
echo "OK: aggregate AOT preserves namespace-qualified global variable slots."
