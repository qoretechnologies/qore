#!/bin/bash
# Regression: aggregate AOT must preserve explicit typed returns after context
# blocks.  This mirrors Qorus AbstractLogger::getLoggerMap(), where a method
# iterates query rows in a context block and returns a hash literal containing
# locally accumulated hashes.

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

CLASS_SRC="${TMP}/src/context-return-hash.qc"
MAIN_SRC="${TMP}/src/main.q"
AGG_QO="${TMP}/qo/context_return_hash_agg.qo"

cat >"${CLASS_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

public class ContextReturnHash {
    hash<auto> build() {
        hash<auto> rows = {
            "id": (1,),
            "name": ("system",),
        };
        hash<auto> row_map = {};
        hash<string, int> aliases = {};

        context (rows) {
            hash<auto> row = %%;
            row_map{%id} = row;
            aliases{%name} = %id;
        }

        return {
            "rows": row_map,
            "aliases": aliases,
        };
    }
}
QORE

cat >"${MAIN_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

int sub context_return_hash_test() {
    ContextReturnHash obj();
    hash<auto> result = obj.build();
    if (result.rows{1}.name != "system") {
        throw "AOT-CONTEXT-RETURN-HASH-ERROR", sprintf("unexpected rows value: %y", result);
    }
    if (result.aliases.system != 1) {
        throw "AOT-CONTEXT-RETURN-HASH-ERROR", sprintf("unexpected aliases value: %y", result);
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
    --script-aggregate=context_return_hash_agg \
    --script-aggregate-native-registers \
    "${CLASS_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 3: build and run aggregate registration harness ==="
cat >"${TMP}/harness.cpp" <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>

extern "C" void init_context_return_hash_agg_qo(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    qore_aot_script_begin_batch(pgm);
    init_context_return_hash_agg_qo(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "qore_aot_script_end_batch rc=%d: %s\n", flush_rc, qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }
    int rc = qore_run_callable(pgm, "context_return_hash_test", nullptr);
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
    -o "${TMP}/context_return_hash_test"

LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${TMP}/context_return_hash_test"

echo ""
echo "OK: aggregate AOT preserves explicit typed returns after context blocks."
