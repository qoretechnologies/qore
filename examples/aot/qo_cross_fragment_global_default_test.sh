#!/bin/bash
# Regression: native AOT default-argument expressions can reference globals
# declared in a different script fragment in the same batch.
#
# Qorus uses this shape in QorusOptionsBase::getURLFromOption(), whose default
# argument calls Qorus.getLocalAddress() while the global Qorus is declared by
# the executable fragment.  The AOT deserializer must create top-level globals
# before deserializing signatures, otherwise the native default expression is
# materialized as <nothing>.getLocalAddress().

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

QCC="./build/qcc"
SRC="${TMP}/src"
mkdir -p "${SRC}/lib" "${SRC}/bin"

cat > "${SRC}/lib/defaults.qc" <<'QORE'
%modern
%require-our

class UsesGlobalDefault {
    static string get(string prefix, string host = App.getLocalAddress()) {
        return prefix + ":" + host;
    }
}
QORE

cat > "${SRC}/bin/main.qr" <<'QORE'
%modern
%require-our

our AppMain App;

class AppMain {
    string getLocalAddress() {
        return "batch-global";
    }
}

int sub verify() {
    App = new AppMain();
    string value = UsesGlobalDefault::get("ok");
    if (value != "ok:batch-global") {
        throw "AOT-GLOBAL-DEFAULT-ERROR",
            sprintf("got %y; expected %y", value, "ok:batch-global");
    }
    return 0;
}
QORE

source_id() {
    realpath "$1" | sed 's/[^A-Za-z0-9_]/_/g'
}

LIB_SRC="${SRC}/lib/defaults.qc"
MAIN_SRC="${SRC}/bin/main.qr"
LIB_ID="$(source_id "${LIB_SRC}")"
MAIN_ID="$(source_id "${MAIN_SRC}")"
LIB_QO="${TMP}/${LIB_ID}.qo"
MAIN_QO="${TMP}/${MAIN_ID}.qo"

"${QCC}" -c --output-dir="${TMP}" "${LIB_SRC}" "${MAIN_SRC}" >/dev/null

cat > "${TMP}/runner.cpp" <<EOF
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>

extern "C" void qore_${LIB_ID}_${LIB_ID}_script_register(QoreProgram*);
extern "C" void qore_${MAIN_ID}_${MAIN_ID}_script_register(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES | PO_REQUIRE_OUR);
    if (!pgm) {
        fprintf(stderr, "qore_create_program failed\n");
        qore_cleanup();
        return 1;
    }

    qore_aot_script_begin_batch(pgm);
    qore_${LIB_ID}_${LIB_ID}_script_register(pgm);
    qore_${MAIN_ID}_${MAIN_ID}_script_register(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "batch flush failed: %s\n", qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }

    int rc = qore_run_callable(pgm, "verify", nullptr);
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
EOF

g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/runner.cpp" "${LIB_QO}" "${MAIN_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/qo_cross_fragment_global_default_test"

LD_LIBRARY_PATH=build "${TMP}/qo_cross_fragment_global_default_test"

echo "OK: cross-fragment global default argument resolved"
