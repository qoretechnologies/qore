#!/bin/bash
# Regression: source-stripped aggregate AOT must preserve method bodies that
# mutate, read, and return keys from an initialized typed object-valued member
# hash.  Qorus RBAC uses this shape in OMQ::RbacProviders::{add,get,getList}().

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

REGISTRY_SRC="${TMP}/src/registry.qc"
MAIN_SRC="${TMP}/src/main.q"
AGG_QO="${TMP}/qo/keys_member_hash_slot_agg.qo"

cat >"${REGISTRY_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

public namespace OMQ {
    public class AbstractProvider {
        string getName() {
            return "db";
        }
    }

    public class ProviderRegistry {
        private {
            hash<string, AbstractProvider> src = {};
        }

        add(AbstractProvider provider) {
            string name = provider.getName();
            if (src{name}) {
                throw "DUPLICATE-PROVIDER", sprintf("provider %y already registered", name);
            }
            src{name} = provider;
        }

        AbstractProvider get(string name) {
            if (!src{name}) {
                throw "MISSING-PROVIDER", sprintf("provider %y is missing", name);
            }
            return src{name};
        }

        list<string> getList() {
            return keys src;
        }
    }
}
QORE

cat >"${MAIN_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

int sub keys_member_hash_slot_test() {
    OMQ::ProviderRegistry registry();
    list<string> names = registry.getList();
    hash<auto> seen = map {$1: True}, names;
    if (seen.size() != 0) {
        throw "AOT-KEYS-MEMBER-HASH-SLOT-ERROR", sprintf("unexpected provider keys: %y", names);
    }
    registry.add(new OMQ::AbstractProvider());
    names = registry.getList();
    seen = map {$1: True}, names;
    if (seen.size() != 1 || !seen.db) {
        throw "AOT-KEYS-MEMBER-HASH-SLOT-ERROR", sprintf("provider key was not registered: %y", names);
    }
    if (registry.get("db").getName() != "db") {
        throw "AOT-KEYS-MEMBER-HASH-SLOT-ERROR", "registered provider could not be retrieved";
    }
    return 0;
}
QORE

REGISTRY_ID="$(source_id "${REGISTRY_SRC}")"
MAIN_ID="$(source_id "${MAIN_SRC}")"
REGISTRY_QO="${TMP}/qo/${REGISTRY_ID}.qo"
MAIN_QO="${TMP}/qo/${MAIN_ID}.qo"

echo "=== Step 1: batch-compile per-file script objects ==="
"${QCC}" -c --output-dir="${TMP}/qo" \
    "${REGISTRY_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 2: compile native-registering script aggregate ==="
"${QCC}" -c \
    -o "${AGG_QO}" \
    --script-aggregate=keys_member_hash_slot_agg \
    --script-aggregate-native-registers \
    "${REGISTRY_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 3: build and run aggregate registration harness ==="
cat >"${TMP}/harness.cpp" <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>

extern "C" void init_keys_member_hash_slot_agg_qo(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    qore_aot_script_begin_batch(pgm);
    init_keys_member_hash_slot_agg_qo(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "qore_aot_script_end_batch rc=%d: %s\n", flush_rc, qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }
    int rc = qore_run_callable(pgm, "keys_member_hash_slot_test", nullptr);
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
EOF

g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/harness.cpp" \
    "${AGG_QO}" \
    "${REGISTRY_QO}" \
    "${MAIN_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/keys_member_hash_slot_test"

LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${TMP}/keys_member_hash_slot_test"

echo ""
echo "OK: aggregate AOT preserves mutation and keys from typed member hashes."
