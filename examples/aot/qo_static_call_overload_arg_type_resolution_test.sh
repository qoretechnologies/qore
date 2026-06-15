#!/bin/bash
# Regression: aggregate native slot registration must preserve static-method
# overload resolution when parse-time only serialized argument type metadata.

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

QCC="${QCC:-./build/qcc}"
mkdir -p "${TMP}/src" "${TMP}/qo"

SRC="${TMP}/src/overload-resolution.q"
SRC_ID="$(realpath "${SRC}" | sed 's/[^A-Za-z0-9_]/_/g')"
SRC_QO="${TMP}/qo/${SRC_ID}.qo"
AGG_QO="${TMP}/qo/overload_resolution_agg.qo"

cat >"${SRC}" <<'QORE'
%new-style
%strict-args
%require-types

public class TypeDescription {
}

public class AbstractDataField {
    public {
        string value;
    }

    constructor(string v) {
        value = v;
    }

    string getValue() {
        return value;
    }
}

public class ConcreteDataField inherits AbstractDataField {
    constructor(string v) : AbstractDataField(v) {
    }
}

public class DataProviderAppActionRestClass {
    public static string doGetTypeDescription(hash<auto> arginfo, *TypeDescription type, auto options) {
        return "object";
    }

    public static string doGetTypeDescription(hash<auto> arginfo, hash<string, AbstractDataField> fields,
            auto args, *bool unsatisfied_deps) {
        return fields{"keep"}.getValue();
    }
}

int sub static_call_overload_arg_type_resolution_test() {
    hash<auto> arginfo = {};
    hash<string, AbstractDataField> fields = {
        "drop": new ConcreteDataField("removed"),
        "keep": new ConcreteDataField("hash"),
    };
    list<string> action_options = ("drop",);

    auto runtime_fields = fields - action_options;
    auto runtime_options = {};

    string result = DataProviderAppActionRestClass::doGetTypeDescription(arginfo, runtime_fields,
        runtime_options);
    if (result != "hash") {
        throw "AOT-STATIC-CALL-OVERLOAD-ERROR", sprintf("unexpected overload result: %y", result);
    }
    return 0;
}
QORE

echo "=== Step 1: compile source script object ==="
"${QCC}" -c -o "${SRC_QO}" "${SRC}" | tail -6

echo ""
echo "=== Step 2: compile native-registering script aggregate ==="
"${QCC}" -c \
    -o "${AGG_QO}" \
    --script-aggregate=overload_resolution_agg \
    --script-aggregate-native-registers \
    "${SRC}" | tail -6

echo ""
echo "=== Step 3: build and run aggregate registration harness ==="
cat >"${TMP}/harness.cpp" <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>

extern "C" void init_overload_resolution_agg_qo(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    qore_aot_script_begin_batch(pgm);
    init_overload_resolution_agg_qo(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "qore_aot_script_end_batch rc=%d: %s\n", flush_rc, qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }
    int rc = qore_run_callable(pgm, "static_call_overload_arg_type_resolution_test", nullptr);
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
EOF

g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/harness.cpp" \
    "${AGG_QO}" \
    "${SRC_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/overload_resolution_test"

LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${TMP}/overload_resolution_test"

echo ""
echo "OK: aggregate native slot registration preserves static-call overload resolution."
