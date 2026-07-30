#!/bin/bash
# Regression: a source-deferred base class constant can contain a nested
# reference to an inherited subclass constant compiled later.  The base
# constant's init function may run before the subclass constant is initialized;
# after first-pass init settles, the stored container must materialize the
# nested RuntimeConstantRefNode and re-apply the declared hashdecl type.

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

BASE_SRC="${TMP}/src/base.qc"
CHILD_SRC="${TMP}/src/child.qc"
MAIN_SRC="${TMP}/src/main.q"
AGG_QO="${TMP}/qo/source_deferred_inherited_const_type_agg.qo"

cat >"${BASE_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

public namespace N {
    public hashdecl ApiOptionInfo {
        string type;
        string desc;
    }

    public class Base {
        public {
            const hash<string, hash<string, hash<ApiOptionInfo>>> ApiProfileOptionsMap = {
                "opcua": Child::ProfileOptions,
            };
        }

        constructor() {
            *hash<string, hash<ApiOptionInfo>> options = ApiProfileOptionsMap{"opcua"};
            if (!options || options.endpoint.type != "string") {
                throw "AOT-INHERITED-CONST-TYPE-ERROR",
                    sprintf("invalid materialized options: %y", options);
            }
        }
    }
}
QORE

cat >"${CHILD_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

public namespace N {
    public class Child inherits Base {
        public {
            const hash<string, hash<ApiOptionInfo>> ProfileOptions = {
                "endpoint": <ApiOptionInfo>{
                    "type": "string",
                    "desc": "Endpoint URL",
                },
            };
        }
    }
}
QORE

cat >"${MAIN_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

int sub source_deferred_inherited_const_type_test() {
    N::Child child();
    return 0;
}
QORE

cat >"${TMP}/source-symbols.manifest" <<QORE
format=1
hashdecl	N::ApiOptionInfo	${BASE_SRC}
class	N::Base	${BASE_SRC}
class	N::Child	${CHILD_SRC}
QORE

BASE_ID="$(source_id "${BASE_SRC}")"
CHILD_ID="$(source_id "${CHILD_SRC}")"
MAIN_ID="$(source_id "${MAIN_SRC}")"
BASE_QO="${TMP}/qo/${BASE_ID}.qo"
CHILD_QO="${TMP}/qo/${CHILD_ID}.qo"
MAIN_QO="${TMP}/qo/${MAIN_ID}.qo"

echo "=== Step 1: compile base before child with source-symbol fallback ==="
"${QCC}" -c \
    -L "${TMP}/qo" \
    --source-symbol-manifest="${TMP}/source-symbols.manifest" \
    --write-index-json="${TMP}/base.idx.json" \
    -o "${BASE_QO}" \
    "${BASE_SRC}" | tail -2

echo ""
echo "=== Step 2: compile child and main against per-file objects ==="
"${QCC}" -c \
    -L "${TMP}/qo" \
    --source-symbol-manifest="${TMP}/source-symbols.manifest" \
    --write-index-json="${TMP}/child.idx.json" \
    -o "${CHILD_QO}" \
    "${CHILD_SRC}" | tail -2
"${QCC}" -c \
    -L "${TMP}/qo" \
    --source-symbol-manifest="${TMP}/source-symbols.manifest" \
    --write-index-json="${TMP}/main.idx.json" \
    -o "${MAIN_QO}" \
    "${MAIN_SRC}" | tail -2

echo ""
echo "=== Step 3: compile native-registering script aggregate ==="
"${QCC}" -c \
    -o "${AGG_QO}" \
    --script-aggregate=source_deferred_inherited_const_type_agg \
    --script-aggregate-native-registers \
    "${BASE_SRC}" \
    "${CHILD_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 4: build and run aggregate registration harness ==="
cat >"${TMP}/harness.cpp" <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>

extern "C" void init_source_deferred_inherited_const_type_agg_qo(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    qore_aot_script_begin_batch(pgm);
    init_source_deferred_inherited_const_type_agg_qo(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "qore_aot_script_end_batch rc=%d: %s\n", flush_rc, qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }
    int rc = qore_run_callable(pgm, "source_deferred_inherited_const_type_test", nullptr);
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
EOF

g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/harness.cpp" \
    "${AGG_QO}" \
    "${BASE_QO}" \
    "${CHILD_QO}" \
    "${MAIN_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/source_deferred_inherited_const_type_test"

LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${TMP}/source_deferred_inherited_const_type_test"

echo ""
echo "OK: source-deferred inherited constants materialize nested typed hashdecl references."
