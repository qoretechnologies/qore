#!/bin/bash
# Regression test for deserialized debug IR lvalue roots.
#
# Source-stripped AOT code can execute serialized debug IR when the target
# program allows debugger attachment.  A global lvalue path such as `shift ARGV`
# must rebind its Var* while deserializing that IR; otherwise optimized builds
# dereference a null Var* in LValueHelper::navigatePath().

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

cat >"${TMP}/main.qr" <<'QORE'
%modern

class C {
    constructor() {
        *string first = shift ARGV;
        if (first != "arg1") {
            throw "AOT-LVALUE-ERROR", sprintf("first arg: %y", first);
        }
        if (ARGV[0] != "arg2") {
            throw "AOT-LVALUE-ERROR", sprintf("remaining ARGV: %y", ARGV);
        }
    }
}
QORE

cat >"${TMP}/host.cpp" <<'CPP'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>

#include <stdio.h>

extern "C" void qore_main_main_script_register(QoreProgram*);

int main(int argc, char** argv) {
    qore_init(QL_GPL, "UTF-8", true);
    qore_setup_argv(1, argc, argv);

    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_ALLOW_DEBUGGER);
    if (!pgm) {
        fprintf(stderr, "qore_create_program failed\n");
        qore_cleanup();
        return 1;
    }

    qore_aot_script_begin_batch(pgm);
    qore_main_main_script_register(pgm);
    int rc = qore_aot_script_end_batch(pgm);
    if (rc != 0) {
        fprintf(stderr, "batch flush failed: %s\n", qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }

    ExceptionSink xsink;
    pgm->runClass("C", &xsink);
    if (xsink) {
        xsink.handleExceptions();
        rc = 1;
    }

    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
CPP

QCC="./build/qcc"

LD_LIBRARY_PATH=build "${QCC}" -c -o "${TMP}/main.qo" "${TMP}/main.qr" >/dev/null

g++ -std=c++20 -Iinclude -Ibuild -Ibuild/include -Lbuild \
    "${TMP}/host.cpp" "${TMP}/main.qo" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/host"

LD_LIBRARY_PATH=build "${TMP}/host" arg1 arg2

echo "OK: debug IR global lvalue path resolves ARGV"
