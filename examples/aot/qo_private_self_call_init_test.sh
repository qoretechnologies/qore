#!/bin/bash
# Regression: aggregate AOT must preserve same-class private self method calls
# from an init-style public method.  This mirrors the Qorus RBAC::init()
# call shape where reloadIntern(False) must execute during qorus-core startup.

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

CLASS_SRC="${TMP}/src/private-self-call.qc"
MAIN_SRC="${TMP}/src/main.q"
AGG_QO="${TMP}/qo/private_self_call_init_agg.qo"

cat >"${CLASS_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

public class PrivateSelfCallInitBase {
    private {
        int count = 0;
        int lock_depth = 0;
    }

    public init() {
        lock();
        on_exit unlock();
        reloadIntern(False);
    }

    public int getCount() {
        return count;
    }

    private reloadIntern(bool enforce_changes = True, *hash<string, hash<auto>> lcm,
            *bool set_limit_classes) {
        if (enforce_changes) {
            throw "AOT-PRIVATE-SELF-CALL-ERROR", "unexpected enforce_changes=True";
        }
        if (lock_depth != 1) {
            throw "AOT-PRIVATE-SELF-CALL-ERROR", sprintf("unexpected lock depth: %d", lock_depth);
        }
        ++count;
    }

    private lock() {
        ++lock_depth;
    }

    private unlock() {
        --lock_depth;
    }
}

public class PrivateSelfCallInitDerived inherits PrivateSelfCallInitBase {
    public init() {
        PrivateSelfCallInitBase::init();
    }
}
QORE

cat >"${MAIN_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

int sub private_self_call_init_test() {
    PrivateSelfCallInitDerived obj();
    obj.init();
    if (obj.getCount() != 1) {
        throw "AOT-PRIVATE-SELF-CALL-ERROR", sprintf("private self call was not executed: %d", obj.getCount());
    }
    return 0;
}
QORE

CLASS_ID="$(source_id "${CLASS_SRC}")"
MAIN_ID="$(source_id "${MAIN_SRC}")"
CLASS_QO="${TMP}/qo/${CLASS_ID}.qo"
MAIN_QO="${TMP}/qo/${MAIN_ID}.qo"

echo "=== Step 1: batch-compile per-file script objects ==="
"${QCC}" -c --output-dir="${TMP}/qo" \
    "${CLASS_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 2: compile native-registering script aggregate ==="
"${QCC}" -c \
    -o "${AGG_QO}" \
    --script-aggregate=private_self_call_init_agg \
    --script-aggregate-native-registers \
    "${CLASS_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 3: build and run aggregate registration harness ==="
cat >"${TMP}/harness.cpp" <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>

extern "C" void init_private_self_call_init_agg_qo(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    qore_aot_script_begin_batch(pgm);
    init_private_self_call_init_agg_qo(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "qore_aot_script_end_batch rc=%d: %s\n", flush_rc, qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }
    int rc = qore_run_callable(pgm, "private_self_call_init_test", nullptr);
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
EOF

g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/harness.cpp" \
    "${AGG_QO}" \
    "${CLASS_QO}" \
    "${MAIN_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/private_self_call_init_test"

LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${TMP}/private_self_call_init_test"

echo ""
echo "OK: aggregate AOT preserves same-class private self calls from init methods."
