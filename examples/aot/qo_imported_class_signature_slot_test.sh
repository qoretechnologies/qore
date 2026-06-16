#!/bin/bash
# Regression: aggregate AOT slot registration must bind methods whose
# signatures use a bare class name resolved from another namespace.  This
# mirrors Qorus RbacProviders::add(AbstractRbacProvider), where the parsed
# signature can be serialized with an imported alias while the runtime variant
# carries the defining namespace path.

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

PROVIDER_SRC="${TMP}/src/provider-types.qc"
REGISTRY_SRC="${TMP}/src/provider-registry.qc"
MAIN_SRC="${TMP}/src/main.q"
AGG_QO="${TMP}/qo/imported_class_signature_agg.qo"

cat >"${PROVIDER_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

public namespace ProviderTypes {
    public class AbstractProvider {
        private {
            string name;
        }

        constructor(string n) {
            name = n;
        }

        string getName() {
            return name;
        }
    }
}
QORE

cat >"${REGISTRY_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

public namespace OMQ {
    public class ProviderRegistry {
        private {
            string seen = "";
        }

        add(AbstractProvider provider) {
            seen = provider.getName();
        }

        string getSeen() {
            return seen;
        }
    }
}
QORE

cat >"${MAIN_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

int sub imported_class_signature_test() {
    ProviderTypes::AbstractProvider provider("db");
    OMQ::ProviderRegistry registry();
    registry.add(provider);
    if (registry.getSeen() != "db") {
        throw "AOT-IMPORTED-CLASS-SIGNATURE-ERROR",
            sprintf("provider method body was not executed: %y", registry.getSeen());
    }
    return 0;
}
QORE

PROVIDER_ID="$(source_id "${PROVIDER_SRC}")"
REGISTRY_ID="$(source_id "${REGISTRY_SRC}")"
MAIN_ID="$(source_id "${MAIN_SRC}")"
PROVIDER_QO="${TMP}/qo/${PROVIDER_ID}.qo"
REGISTRY_QO="${TMP}/qo/${REGISTRY_ID}.qo"
MAIN_QO="${TMP}/qo/${MAIN_ID}.qo"

echo "=== Step 1: batch-compile per-file script objects ==="
"${QCC}" -c --output-dir="${TMP}/qo" \
    "${PROVIDER_SRC}" \
    "${REGISTRY_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 2: compile native-registering script aggregate ==="
"${QCC}" -c \
    -o "${AGG_QO}" \
    --script-aggregate=imported_class_signature_agg \
    --script-aggregate-native-registers \
    "${PROVIDER_SRC}" \
    "${REGISTRY_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 3: build and run aggregate registration harness ==="
cat >"${TMP}/harness.cpp" <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>

extern "C" void init_imported_class_signature_agg_qo(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    qore_aot_script_begin_batch(pgm);
    init_imported_class_signature_agg_qo(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "qore_aot_script_end_batch rc=%d: %s\n", flush_rc, qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }
    int rc = qore_run_callable(pgm, "imported_class_signature_test", nullptr);
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
EOF

g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/harness.cpp" \
    "${AGG_QO}" \
    "${PROVIDER_QO}" \
    "${REGISTRY_QO}" \
    "${MAIN_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/imported_class_signature_test"

LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${TMP}/imported_class_signature_test"

echo ""
echo "OK: aggregate AOT binds native bodies for bare imported class signature types."
