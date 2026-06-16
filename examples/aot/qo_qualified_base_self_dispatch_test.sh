#!/bin/bash
# Regression: aggregate AOT must preserve explicit base-qualified self calls.
# A call such as Base::setupEncryption() from an overriding Derived method is
# non-virtual; serializing it as an unqualified self call dispatches back to the
# override and can recurse indefinitely.

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
cleanup() {
    if [[ -n "${KEEP_TMP:-}" ]]; then
        echo "kept temp dir: ${TMP}" >&2
    else
        rm -rf "${TMP}"
    fi
}
trap cleanup EXIT

QCC="${QCC:-./build/qcc}"
mkdir -p "${TMP}/src" "${TMP}/qo"

source_id() {
    realpath "$1" | sed 's/[^A-Za-z0-9_]/_/g'
}

BASE_SRC="${TMP}/src/crypto-key-helper.qc"
APP_SRC="${TMP}/src/qorus-app.qc"
MAIN_SRC="${TMP}/src/main.q"
AGG_QO="${TMP}/qo/qualified_base_self_dispatch_agg.qo"

cat >"${BASE_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

namespace OMQ {
public class CryptoKeyHelper {
    private string setupEncryption() {
        return "base";
    }
}
}
QORE

cat >"${APP_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

namespace OMQ {
public class QorusApp inherits CryptoKeyHelper {
    public string run() {
        return setupEncryption();
    }

    private:internal string setupEncryption() {
        return CryptoKeyHelper::setupEncryption();
    }
}
}
QORE

cat >"${MAIN_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

int sub qualified_base_self_dispatch_test() {
    OMQ::QorusApp app();
    string value = app.run();
    if (value != "base") {
        throw "AOT-DISPATCH-ERROR", sprintf("expected base dispatch, got '%s'", value);
    }
    printf("%s\n", value);
    return 0;
}
QORE

BASE_ID="$(source_id "${BASE_SRC}")"
APP_ID="$(source_id "${APP_SRC}")"
MAIN_ID="$(source_id "${MAIN_SRC}")"
BASE_QO="${TMP}/qo/${BASE_ID}.qo"
APP_QO="${TMP}/qo/${APP_ID}.qo"
MAIN_QO="${TMP}/qo/${MAIN_ID}.qo"

echo "=== Step 1: batch-compile per-file script objects ==="
"${QCC}" -c --output-dir="${TMP}/qo" \
    "${BASE_SRC}" \
    "${APP_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 2: compile native-registering script aggregate ==="
"${QCC}" -c \
    -o "${AGG_QO}" \
    --script-aggregate=qualified_base_self_dispatch_agg \
    --script-aggregate-native-registers \
    "${BASE_SRC}" \
    "${APP_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 3: build and run aggregate registration harness ==="
cat >"${TMP}/harness.cpp" <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>

extern "C" void init_qualified_base_self_dispatch_agg_qo(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    qore_aot_script_begin_batch(pgm);
    init_qualified_base_self_dispatch_agg_qo(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "qore_aot_script_end_batch rc=%d: %s\n", flush_rc, qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }
    int rc = qore_run_callable(pgm, "qualified_base_self_dispatch_test", nullptr);
    if (rc != 0) {
        fprintf(stderr, "qualified_base_self_dispatch_test rc=%d: %s\n", rc, qore_last_error(pgm));
    }
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
EOF

g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/harness.cpp" \
    "${AGG_QO}" \
    "${BASE_QO}" \
    "${APP_QO}" \
    "${MAIN_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/qualified_base_self_dispatch_test"

LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${TMP}/qualified_base_self_dispatch_test"

echo ""
echo "OK: aggregate AOT preserves explicit base-qualified self method dispatch."

echo ""
echo "=== Step 4: compile source-symbol-deferred namespace-relative provider ==="
DEFER_TMP="${TMP}/deferred"
mkdir -p "${DEFER_TMP}/src" "${DEFER_TMP}/qo"
DEFER_PROVIDER_SRC="${DEFER_TMP}/src/crypto-key-helper.qc"
DEFER_CONSUMER_SRC="${DEFER_TMP}/src/qorus-app.qc"
DEFER_PROVIDER_QO="${DEFER_TMP}/qo/crypto-key-helper.qo"
DEFER_CONSUMER_QO="${DEFER_TMP}/qo/qorus-app.qo"

cat >"${DEFER_PROVIDER_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

namespace OMQ {
public class CryptoKeyHelper {
    private string setupEncryption() {
        return "base";
    }
}
}
QORE

cat >"${DEFER_CONSUMER_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

namespace OMQ {
public class QorusApp inherits CryptoKeyHelper {
    public string run() {
        return setupEncryption();
    }

    private:internal string setupEncryption() {
        return CryptoKeyHelper::setupEncryption();
    }
}
}

int sub deferred_qualified_base_self_dispatch_test() {
    OMQ::QorusApp app();
    string value = app.run();
    if (value != "base") {
        throw "AOT-DISPATCH-ERROR", sprintf("expected base dispatch, got '%s'", value);
    }
    printf("%s\n", value);
    return 0;
}
QORE

"${QCC}" -c -o "${DEFER_PROVIDER_QO}" "${DEFER_PROVIDER_SRC}" | tail -2

cat >"${DEFER_TMP}/source-symbols.manifest" <<QORE
format=1
class	OMQ::CryptoKeyHelper	${DEFER_PROVIDER_SRC}
QORE

echo ""
echo "=== Step 5: compile source-symbol-deferred namespace-relative consumer ==="
"${QCC}" -c \
    -L "${DEFER_TMP}/qo" \
    --source-symbol-manifest="${DEFER_TMP}/source-symbols.manifest" \
    --write-index-json="${DEFER_TMP}/consumer.idx.json" \
    -o "${DEFER_CONSUMER_QO}" \
    "${DEFER_CONSUMER_SRC}" | tail -2

grep -q 'CryptoKeyHelper::setupEncryption' "${DEFER_TMP}/consumer.idx.json"

echo ""
echo "=== Step 6: link deferred namespace-relative objects and run ==="
"${QCC}" -e deferred_qualified_base_self_dispatch_test \
    -o "${DEFER_TMP}/deferred_dispatch" \
    "${DEFER_PROVIDER_QO}" \
    "${DEFER_CONSUMER_QO}" | tail -2
out="$(LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${DEFER_TMP}/deferred_dispatch")"
test "${out}" = "base"
printf '%s\n' "${out}"

echo ""
echo "OK: source-symbol-deferred namespace-relative base calls dispatch non-virtually."
