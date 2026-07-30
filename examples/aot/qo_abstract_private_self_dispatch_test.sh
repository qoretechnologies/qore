#!/bin/bash
# Regression: aggregate AOT must preserve unqualified self-call virtual dispatch
# from a base-class method to a private abstract method implemented by a
# subclass.  Running the abstract placeholder body returns NOTHING and fails
# the non-optional return type check.

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

BASE_SRC="${TMP}/src/base-manager.qc"
DERIVED_SRC="${TMP}/src/datasource-manager.qc"
MAIN_SRC="${TMP}/src/main.q"
AGG_QO="${TMP}/qo/abstract_private_self_dispatch_agg.qo"

cat >"${BASE_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

public class PoolHandle {
    public {
        string name;
    }

    constructor(string n) {
        name = n;
    }
}

public class AbstractDatasourceManager {
    public PoolHandle getPool(string name) {
        PoolHandle dsp = getPoolImpl(name);
        return dsp;
    }

    private abstract PoolHandle getPoolImpl(string name);
}
QORE

cat >"${DERIVED_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

public class DatasourceManager inherits AbstractDatasourceManager {
    private PoolHandle getPoolImpl(string name) {
        return new PoolHandle("derived:" + name);
    }
}
QORE

cat >"${MAIN_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

int sub abstract_private_self_dispatch_test() {
    DatasourceManager mgr();
    PoolHandle dsp = mgr.getPool("omq");
    if (dsp.name != "derived:omq") {
        throw "AOT-ABSTRACT-PRIVATE-SELF-DISPATCH-ERROR", sprintf("unexpected result: %y", dsp.name);
    }
    return 0;
}
QORE

BASE_ID="$(source_id "${BASE_SRC}")"
DERIVED_ID="$(source_id "${DERIVED_SRC}")"
MAIN_ID="$(source_id "${MAIN_SRC}")"
BASE_QO="${TMP}/qo/${BASE_ID}.qo"
DERIVED_QO="${TMP}/qo/${DERIVED_ID}.qo"
MAIN_QO="${TMP}/qo/${MAIN_ID}.qo"

echo "=== Step 1: batch-compile per-file script objects ==="
"${QCC}" -c --output-dir="${TMP}/qo" \
    "${BASE_SRC}" \
    "${DERIVED_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 2: compile native-registering script aggregate ==="
"${QCC}" -c \
    -o "${AGG_QO}" \
    --script-aggregate=abstract_private_self_dispatch_agg \
    --script-aggregate-native-registers \
    "${BASE_SRC}" \
    "${DERIVED_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 3: build and run aggregate registration harness ==="
cat >"${TMP}/harness.cpp" <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>

extern "C" void init_abstract_private_self_dispatch_agg_qo(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    qore_aot_script_begin_batch(pgm);
    init_abstract_private_self_dispatch_agg_qo(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "qore_aot_script_end_batch rc=%d: %s\n", flush_rc, qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }
    int rc = qore_run_callable(pgm, "abstract_private_self_dispatch_test", nullptr);
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
EOF

g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/harness.cpp" \
    "${AGG_QO}" \
    "${BASE_QO}" \
    "${DERIVED_QO}" \
    "${MAIN_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/abstract_private_self_dispatch_test"

LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${TMP}/abstract_private_self_dispatch_test"

echo ""
echo "OK: aggregate AOT preserves private abstract self-call dispatch."
