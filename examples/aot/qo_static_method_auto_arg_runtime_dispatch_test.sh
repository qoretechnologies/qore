#!/bin/bash
# Regression: weak AOT static-call argument metadata must preserve runtime
# overload dispatch instead of binding an arbitrary parse-time variant.

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

QCC="${QCC:-./build/qcc}"
MODULE_DIR="${QORE_ROOT}/qlib${QORE_MODULE_DIR:+:${QORE_MODULE_DIR}}"
mkdir -p "${TMP}/src" "${TMP}/qo"

SRC="${TMP}/src/static-method-auto-arg-runtime-dispatch.q"
SRC_ID="$(realpath "${SRC}" | sed 's/[^A-Za-z0-9_]/_/g')"
SRC_QO="${TMP}/qo/${SRC_ID}.qo"
AGG_QO="${TMP}/qo/static_method_auto_arg_runtime_dispatch_agg.qo"

cat >"${SRC}" <<'QORE'
%new-style
%strict-args
%require-types
%requires RestHandler

auto sub make_runtime_rest_body() {
    return {"id": 77, "source": "runtime"};
}

int sub static_method_auto_arg_runtime_dispatch_test() {
    hash<HttpHandlerResponseInfo> response = RestHandler::makeResponse(201, make_runtime_rest_body());
    if (response.code != 201 || response.body.id != 77 || response.body.source != "runtime") {
        throw "AOT-STATIC-CALL-RUNTIME-DISPATCH-ERROR", sprintf("unexpected response: %y", response);
    }
    return 0;
}
QORE

echo "=== Step 1: compile source script object ==="
QORE_LIBDIR="${QORE_ROOT}/build" QORE_MODULE_DIR="${MODULE_DIR}" \
    "${QCC}" -c -o "${SRC_QO}" "${SRC}" | tail -6

echo ""
echo "=== Step 2: compile native-registering script aggregate ==="
QORE_LIBDIR="${QORE_ROOT}/build" QORE_MODULE_DIR="${MODULE_DIR}" \
    "${QCC}" -c \
    -o "${AGG_QO}" \
    --script-aggregate=static_method_auto_arg_runtime_dispatch_agg \
    --script-aggregate-native-registers \
    "${SRC}" | tail -6

echo ""
echo "=== Step 3: build and run aggregate registration harness ==="
cat >"${TMP}/harness.cpp" <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <qore/ModuleManager.h>
#include <stdio.h>

extern "C" void init_static_method_auto_arg_runtime_dispatch_agg_qo(QoreProgram*);

static int fail(QoreProgram* pgm, const char* what) {
    fprintf(stderr, "%s: %s\n", what, qore_last_error(pgm));
    qore_destroy_program(pgm);
    qore_cleanup();
    return 1;
}

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);

    ExceptionSink xsink;
    if (ModuleManager::runTimeLoadModule("RestHandler", pgm, &xsink) || xsink) {
        xsink.handleExceptions();
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }

    qore_aot_script_begin_batch(pgm);
    init_static_method_auto_arg_runtime_dispatch_agg_qo(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        return fail(pgm, "qore_aot_script_end_batch failed");
    }

    int rc = qore_run_callable(pgm, "static_method_auto_arg_runtime_dispatch_test", nullptr);
    if (rc != 0) {
        fprintf(stderr, "qore_run_callable failed: %s\n", qore_last_error(pgm));
    }
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
    -o "${TMP}/static_method_auto_arg_runtime_dispatch_test"

LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    QORE_LIBDIR="${QORE_ROOT}/build" \
    QORE_MODULE_DIR="${MODULE_DIR}" \
    "${TMP}/static_method_auto_arg_runtime_dispatch_test"

echo ""
echo "OK: weak static-call argument metadata preserves runtime overload dispatch."
