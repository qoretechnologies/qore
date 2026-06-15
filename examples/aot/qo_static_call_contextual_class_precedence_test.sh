#!/bin/bash
# Regression: a bare class receiver in a namespace must resolve like source
# parsing.  The namespace-local class wins over an unrelated global class with
# the same simple name, including during aggregate native slot-map registration.

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

GLOBAL_SRC="${TMP}/src/global-helper.qc"
PROVIDER_SRC="${TMP}/src/remote-development-helper.qc"
CONSUMER_SRC="${TMP}/src/deploy-process-fsa.qc"
MAIN_SRC="${TMP}/src/main.q"
AGG_QO="${TMP}/qo/relative_lookup_agg.qo"

cat >"${GLOBAL_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

public class OptionHelper {
    public static string globalOnly(string token) {
        return "global:" + token;
    }
}
QORE

cat >"${PROVIDER_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

public namespace RemoteDevelopment {
    public class OptionHelper {
        public static string convertTokenToOloadArgument(string token) {
            return "-t=" + token;
        }
    }
}
QORE

cat >"${CONSUMER_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

public namespace RemoteDevelopment {
    public class DeployProcessFSA {
        public static string getProcessArgs(string token) {
            return OptionHelper::convertTokenToOloadArgument(token);
        }
    }
}
QORE

cat >"${MAIN_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

int sub relative_lookup_test() {
    string result = RemoteDevelopment::DeployProcessFSA::getProcessArgs("abc");
    if (result != "-t=abc") {
        throw "AOT-RELATIVE-CLASS-LOOKUP-ERROR", sprintf("unexpected result: %y", result);
    }
    return 0;
}
QORE

GLOBAL_ID="$(source_id "${GLOBAL_SRC}")"
PROVIDER_ID="$(source_id "${PROVIDER_SRC}")"
CONSUMER_ID="$(source_id "${CONSUMER_SRC}")"
MAIN_ID="$(source_id "${MAIN_SRC}")"
GLOBAL_QO="${TMP}/qo/${GLOBAL_ID}.qo"
PROVIDER_QO="${TMP}/qo/${PROVIDER_ID}.qo"
CONSUMER_QO="${TMP}/qo/${CONSUMER_ID}.qo"
MAIN_QO="${TMP}/qo/${MAIN_ID}.qo"

echo "=== Step 1: batch-compile per-file script objects ==="
"${QCC}" -c --output-dir="${TMP}/qo" \
    "${GLOBAL_SRC}" \
    "${PROVIDER_SRC}" \
    "${CONSUMER_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 2: compile native-registering script aggregate ==="
"${QCC}" -c \
    -o "${AGG_QO}" \
    --script-aggregate=relative_lookup_agg \
    --script-aggregate-native-registers \
    "${GLOBAL_SRC}" \
    "${PROVIDER_SRC}" \
    "${CONSUMER_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 3: build and run aggregate registration harness ==="
cat >"${TMP}/harness.cpp" <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>

extern "C" void init_relative_lookup_agg_qo(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    qore_aot_script_begin_batch(pgm);
    init_relative_lookup_agg_qo(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "qore_aot_script_end_batch rc=%d: %s\n", flush_rc, qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }
    int rc = qore_run_callable(pgm, "relative_lookup_test", nullptr);
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
EOF

g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/harness.cpp" \
    "${AGG_QO}" \
    "${GLOBAL_QO}" \
    "${PROVIDER_QO}" \
    "${CONSUMER_QO}" \
    "${MAIN_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/relative_lookup_test"

LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${TMP}/relative_lookup_test"

echo ""
echo "OK: aggregate native slot registration preserves contextual class lookup precedence."
