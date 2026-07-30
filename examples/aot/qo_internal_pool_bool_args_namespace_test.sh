#!/bin/bash
# Regression: source-stripped aggregate AOT must preserve explicit boolean
# arguments and root global lookup for an inherited manager method.  This
# mirrors Qorus AbstractDatasourceManager::getPool("omq", False, False).

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

GLOBALS_SRC="${TMP}/src/globals.qc"
MANAGER_SRC="${TMP}/src/manager.qc"
MAIN_SRC="${TMP}/src/main.q"
AGG_QO="${TMP}/qo/internal_pool_bool_args_namespace_agg.qo"

cat >"${GLOBALS_SRC}" <<'QORE'
%new-style
%strict-args
%require-types
%require-our

public class PoolHandle {
    public {
        string name;
    }

    constructor(string n) {
        name = n;
    }
}

our (
    PoolHandle omqp,
);

public namespace OMQ {
    public namespace UserApi {
        public namespace RbacModule {
            public our PoolHandle omqp;
        }
    }
}
QORE

cat >"${MANAGER_SRC}" <<'QORE'
%new-style

namespace OMQ {
class AbstractManager {
    PoolHandle getPool(string name, bool register_dependency = True, bool extern = True) {
        if (!extern && name == "omq") {
            return omqp;
        }

        PoolHandle dsp = getPoolImpl(name, register_dependency, extern);
        return dsp;
    }

    abstract PoolHandle getPoolImpl(string name, bool register_dependency = True, bool extern = True);
}

class Manager inherits AbstractManager {
    PoolHandle getSqlTableSystem(string datasource, string name) {
        return getPool(datasource, False, False);
    }

    PoolHandle getPoolImpl(string name, bool register_dependency = True, bool extern = True) {
        PoolHandle dsp;
        return dsp;
    }
}
}
QORE

cat >"${MAIN_SRC}" <<'QORE'
%new-style
%strict-args
%require-types
%require-our

int sub internal_pool_bool_args_namespace_test() {
    omqp = new PoolHandle("root");
    OMQ::UserApi::RbacModule::omqp = new PoolHandle("rbac");

    OMQ::Manager mgr();
    PoolHandle direct = mgr.getPool("omq", False, False);
    if (direct.name != "root") {
        throw "AOT-INTERNAL-POOL-ERROR", sprintf("direct getPool returned %y", direct.name);
    }

    PoolHandle nested = mgr.getSqlTableSystem("omq", "workflows");
    if (nested.name != "root") {
        throw "AOT-INTERNAL-POOL-ERROR", sprintf("nested getPool returned %y", nested.name);
    }

    return 0;
}
QORE

GLOBALS_ID="$(source_id "${GLOBALS_SRC}")"
MANAGER_ID="$(source_id "${MANAGER_SRC}")"
MAIN_ID="$(source_id "${MAIN_SRC}")"
GLOBALS_QO="${TMP}/qo/${GLOBALS_ID}.qo"
MANAGER_QO="${TMP}/qo/${MANAGER_ID}.qo"
MAIN_QO="${TMP}/qo/${MAIN_ID}.qo"

echo "=== Step 1: batch-compile per-file script objects ==="
"${QCC}" -c --output-dir="${TMP}/qo" \
    "${MANAGER_SRC}" \
    "${GLOBALS_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 2: compile native-registering script aggregate ==="
"${QCC}" -c \
    -o "${AGG_QO}" \
    --script-aggregate=internal_pool_bool_args_namespace_agg \
    --script-aggregate-native-registers \
    "${MANAGER_SRC}" \
    "${GLOBALS_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 3: build and run aggregate registration harness ==="
cat >"${TMP}/harness.cpp" <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>

extern "C" void init_internal_pool_bool_args_namespace_agg_qo(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES | PO_REQUIRE_OUR);
    qore_aot_script_begin_batch(pgm);
    init_internal_pool_bool_args_namespace_agg_qo(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "qore_aot_script_end_batch rc=%d: %s\n", flush_rc, qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }
    int rc = qore_run_callable(pgm, "internal_pool_bool_args_namespace_test", nullptr);
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
EOF

g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/harness.cpp" \
    "${AGG_QO}" \
    "${GLOBALS_QO}" \
    "${MANAGER_QO}" \
    "${MAIN_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/internal_pool_bool_args_namespace_test"

LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${TMP}/internal_pool_bool_args_namespace_test"

echo ""
echo "OK: aggregate AOT preserves internal pool boolean arguments and namespace globals."
