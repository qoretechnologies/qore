#!/bin/bash
# Regression: script aggregate native registration must resolve constructor
# calls to private classes from the aggregate metadata.  Qorus qorus-core uses
# this shape when per-file native bodies instantiate non-public helper classes
# after aggregate metadata has deserialized the full program.

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

PROVIDER_SRC="${TMP}/src/private-constructor-provider.qc"
MAIN_SRC="${TMP}/src/main.q"
AGG_QO="${TMP}/qo/private_constructor_agg.qo"

cat >"${PROVIDER_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

class PrivateAggregateHelper {
    private {
        string value;
    }

    constructor(string n_value) {
        value = n_value;
    }

    string getValue() {
        return value;
    }
}

public class PrivateAggregateMaker {
    public string makeViaPrivateHelper(string value) {
        PrivateAggregateHelper helper(value);
        return helper.getValue();
    }

    public code<string()> makeDeferredPrivateHelper(string value) {
        return string sub () {
            PrivateAggregateHelper helper(value);
            return helper.getValue();
        };
    }
}
QORE

cat >"${MAIN_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

int sub private_constructor_aggregate_test() {
    PrivateAggregateMaker maker();
    string value = maker.makeViaPrivateHelper("ok");
    if (value != "ok") {
        throw "AOT-PRIVATE-CONSTRUCTOR-ERROR", sprintf("unexpected value: %y", value);
    }
    code<string()> deferred = maker.makeDeferredPrivateHelper("deferred");
    string deferred_value = deferred();
    if (deferred_value != "deferred") {
        throw "AOT-PRIVATE-CONSTRUCTOR-ERROR", sprintf("unexpected deferred value: %y", deferred_value);
    }
    return 0;
}
QORE

PROVIDER_ID="$(source_id "${PROVIDER_SRC}")"
MAIN_ID="$(source_id "${MAIN_SRC}")"
PROVIDER_QO="${TMP}/qo/${PROVIDER_ID}.qo"
MAIN_QO="${TMP}/qo/${MAIN_ID}.qo"

echo "=== Step 1: batch-compile per-file script objects ==="
"${QCC}" -c --output-dir="${TMP}/qo" \
    "${PROVIDER_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 2: compile native-registering script aggregate ==="
"${QCC}" -c \
    -o "${AGG_QO}" \
    --script-aggregate=private_constructor_agg \
    --script-aggregate-native-registers \
    "${PROVIDER_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 3: build and run aggregate registration harness ==="
cat >"${TMP}/harness.cpp" <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>

extern "C" void init_private_constructor_agg_qo(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    qore_aot_script_begin_batch(pgm);
    init_private_constructor_agg_qo(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "qore_aot_script_end_batch rc=%d: %s\n", flush_rc, qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }
    int rc = qore_run_callable(pgm, "private_constructor_aggregate_test", nullptr);
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
EOF

g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/harness.cpp" \
    "${AGG_QO}" \
    "${PROVIDER_QO}" \
    "${MAIN_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/private_constructor_aggregate_test"

LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${TMP}/private_constructor_aggregate_test"

echo ""
echo "OK: aggregate AOT resolves private constructor calls from native slot maps."

echo ""
echo "=== Step 4: run native registration before provider metadata ==="
cat >"${TMP}/delayed-harness.cpp" <<EOF
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>

extern "C" void qore_${MAIN_ID}_${MAIN_ID}_script_register(QoreProgram*);
extern "C" void qore_${PROVIDER_ID}_${PROVIDER_ID}_script_register(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    qore_aot_script_begin_batch(pgm);
    qore_${MAIN_ID}_${MAIN_ID}_script_register(pgm);
    qore_${PROVIDER_ID}_${PROVIDER_ID}_script_register(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "qore_aot_script_end_batch rc=%d: %s\n", flush_rc, qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }
    int rc = qore_run_callable(pgm, "private_constructor_aggregate_test", nullptr);
    if (rc != 0) {
        fprintf(stderr, "private_constructor_aggregate_test rc=%d: %s\n", rc, qore_last_error(pgm));
    }
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
EOF

g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/delayed-harness.cpp" \
    "${MAIN_QO}" \
    "${PROVIDER_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/private_constructor_delayed_test"

LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${TMP}/private_constructor_delayed_test"

echo ""
echo "OK: AOT new-object IR preserves class paths across delayed class resolution."
