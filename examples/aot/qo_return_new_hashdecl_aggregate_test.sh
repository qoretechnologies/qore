#!/bin/bash
# Regression: aggregate AOT must return the newly constructed object from a
# direct `return new Derived(hashdecl_arg)` expression.  Qorus REST classes use
# this shape for versioned response wrappers; a broken new-object result leaks
# NOTHING to the caller and trips the caller's typed return check.

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

TYPES_SRC="${TMP}/src/rest-types.qc"
BASE_SRC="${TMP}/src/rest-base.qc"
V8_SRC="${TMP}/src/rest-v8.qc"
V9_SRC="${TMP}/src/rest-v9.qc"
MAIN_SRC="${TMP}/src/main.q"
AGG_QO="${TMP}/qo/return_new_hashdecl_agg.qo"

cat >"${TYPES_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

public hashdecl RestInfo {
    string name;
}

class QorusRestClass {
    string name() {
        return "base";
    }
}

class UserRestClass inherits QorusRestClass {
    private {
        hash<RestInfo> info;
    }

    constructor(hash<RestInfo> h) {
        info = h;
    }

    string getName() {
        return info.name;
    }

    string name() {
        return info.name;
    }
}
QORE

cat >"${BASE_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

class UsersRestClass inherits QorusRestClass {
    *QorusRestClass subClassImpl(string name) {
        hash<RestInfo> user = <RestInfo>{"name": name};
        return doGetUserRestClass(user);
    }

    private UserRestClass doGetUserRestClass(hash<RestInfo> user) {
        return new UserRestClass(user);
    }
}
QORE

cat >"${V8_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

class UserRestClassV8 inherits UserRestClass {
    constructor(hash<RestInfo> h) : UserRestClass(h) {
    }
}

class UsersRestClassV8 inherits UsersRestClass {
    private UserRestClass doGetUserRestClass(hash<RestInfo> user) {
        return new UserRestClassV8(user);
    }
}
QORE

cat >"${V9_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

class UserRestClassV9 inherits UserRestClassV8 {
    constructor(hash<RestInfo> h) : UserRestClassV8(h) {
    }
}

class UsersRestClassV9 inherits UsersRestClassV8 {
    private UserRestClass doGetUserRestClass(hash<RestInfo> user) {
        return new UserRestClassV9(user);
    }
}
QORE

cat >"${MAIN_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

int sub return_new_hashdecl_aggregate_test() {
    UsersRestClassV9 users();
    *QorusRestClass result = users.subClassImpl("ok");
    if (!result || result.name() != "ok") {
        throw "AOT-RETURN-NEW-HASHDECL-ERROR", sprintf("unexpected result: %y", result);
    }
    return 0;
}
QORE

TYPES_ID="$(source_id "${TYPES_SRC}")"
BASE_ID="$(source_id "${BASE_SRC}")"
V8_ID="$(source_id "${V8_SRC}")"
V9_ID="$(source_id "${V9_SRC}")"
MAIN_ID="$(source_id "${MAIN_SRC}")"
TYPES_QO="${TMP}/qo/${TYPES_ID}.qo"
BASE_QO="${TMP}/qo/${BASE_ID}.qo"
V8_QO="${TMP}/qo/${V8_ID}.qo"
V9_QO="${TMP}/qo/${V9_ID}.qo"
MAIN_QO="${TMP}/qo/${MAIN_ID}.qo"

echo "=== Step 1: batch-compile per-file script objects ==="
"${QCC}" -c --output-dir="${TMP}/qo" \
    "${TYPES_SRC}" \
    "${BASE_SRC}" \
    "${V8_SRC}" \
    "${V9_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 2: compile native-registering script aggregate ==="
"${QCC}" -c \
    -o "${AGG_QO}" \
    --script-aggregate=return_new_hashdecl_agg \
    --script-aggregate-native-registers \
    "${TYPES_SRC}" \
    "${BASE_SRC}" \
    "${V8_SRC}" \
    "${V9_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 3: build and run aggregate registration harness ==="
cat >"${TMP}/harness.cpp" <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>

extern "C" void init_return_new_hashdecl_agg_qo(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    qore_aot_script_begin_batch(pgm);
    init_return_new_hashdecl_agg_qo(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "qore_aot_script_end_batch rc=%d: %s\n", flush_rc, qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }
    int rc = qore_run_callable(pgm, "return_new_hashdecl_aggregate_test", nullptr);
    if (rc != 0) {
        fprintf(stderr, "return_new_hashdecl_aggregate_test rc=%d: %s\n", rc, qore_last_error(pgm));
    }
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
EOF

g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/harness.cpp" \
    "${AGG_QO}" \
    "${TYPES_QO}" \
    "${BASE_QO}" \
    "${V8_QO}" \
    "${V9_QO}" \
    "${MAIN_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/return_new_hashdecl_aggregate_test"

LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${TMP}/return_new_hashdecl_aggregate_test"

echo ""
echo "OK: aggregate AOT returns direct new-object expressions with hashdecl constructor args."
