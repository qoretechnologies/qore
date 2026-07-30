#!/bin/bash
# Regression: an AOT-registered class must resolve its local static method
# before inherited committed static methods while its variants are still
# parse-local during batch deserialization.

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

QCC="${QCC:-./build/qcc}"
MODULE_DIR="${QORE_ROOT}/qlib${QORE_MODULE_DIR:+:${QORE_MODULE_DIR}}"
mkdir -p "${TMP}/src" "${TMP}/qo"

SRC="${TMP}/src/static-local-overrides-inherited.q"
SRC_ID="$(realpath "${SRC}" | sed 's/[^A-Za-z0-9_]/_/g')"
SRC_QO="${TMP}/qo/${SRC_ID}.qo"
AGG_QO="${TMP}/qo/static_local_overrides_inherited_agg.qo"

cat >"${SRC}" <<'QORE'
%new-style
%strict-args
%require-types
%requires RestHandler
%requires HttpServerUtil

public class AotRestResponseHandler inherits HttpServer::AbstractHttpRequestHandler {
    public static hash<auto> makeResponse(int code, auto body, *hash<auto> hdr) {
        return {
            "provider": "child",
            "code": code,
            "body": body,
            "hdr": hdr,
        };
    }
}

int sub static_local_overrides_inherited_committed_test() {
    hash<auto> response = AotRestResponseHandler::makeResponse(201, {"id": 15});
    if (response.provider != "child" || response.code != 201 || response.body.id != 15) {
        throw "AOT-STATIC-LOCAL-RESOLUTION-ERROR", sprintf("unexpected response: %y", response);
    }

    hash<HttpHandlerResponseInfo> real_response = RestHandler::makeResponse(201, {"id": 16});
    if (real_response.code != 201 || real_response.body.id != 16) {
        throw "AOT-RESTHANDLER-STATIC-RESOLUTION-ERROR", sprintf("unexpected response: %y", real_response);
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
    --script-aggregate=static_local_overrides_inherited_agg \
    --script-aggregate-native-registers \
    "${SRC}" | tail -6

echo ""
echo "=== Step 3: build and run aggregate registration harness ==="
cat >"${TMP}/harness.cpp" <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <qore/ModuleManager.h>
#include <stdio.h>

extern "C" void init_static_local_overrides_inherited_agg_qo(QoreProgram*);

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
    init_static_local_overrides_inherited_agg_qo(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        return fail(pgm, "qore_aot_script_end_batch failed");
    }

    int rc = qore_run_callable(pgm, "static_local_overrides_inherited_committed_test", nullptr);
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
    -o "${TMP}/static_local_overrides_inherited_test"

LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    QORE_LIBDIR="${QORE_ROOT}/build" \
    QORE_MODULE_DIR="${MODULE_DIR}" \
    "${TMP}/static_local_overrides_inherited_test"

echo ""
echo "OK: AOT static lookup prefers local parse-time methods before inherited committed methods."
