#!/bin/bash
# Regression test for script-context AOT batch source identity.
#
# The batch compiler must not derive .qo paths or script_register symbols from
# basename-only source names.  Qorus has both lib/qorus.ql and bin/qorus.qr;
# basename-only identity made both emit qorus.qo / qore_qorus_qorus_* and one
# fragment silently replaced the other.

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

mkdir -p "${TMP}/lib" "${TMP}/bin" "${TMP}/out"

cat >"${TMP}/lib/qorus.ql" <<'QORE'
%modern

namespace OMQ {
    public const omq_option_hash = {"answer": 42};
}
QORE

cat >"${TMP}/bin/qorus.qr" <<'QORE'
%modern

int sub verify() {
    if (OMQ::omq_option_hash{"answer"} != 42) {
        throw "AOT-BATCH-ID-ERROR", "cross-fragment constant was not registered";
    }
    return 0;
}
QORE

source_id() {
    realpath "$1" | sed 's/[^A-Za-z0-9_]/_/g'
}

LIB_ID="$(source_id "${TMP}/lib/qorus.ql")"
BIN_ID="$(source_id "${TMP}/bin/qorus.qr")"

QCC="./build/qcc"

LD_LIBRARY_PATH=build "${QCC}" -c --output-dir="${TMP}/out" \
    "${TMP}/lib/qorus.ql" "${TMP}/bin/qorus.qr" >/dev/null

test -f "${TMP}/out/${LIB_ID}.qo"
test -f "${TMP}/out/${BIN_ID}.qo"

nm "${TMP}/out/${LIB_ID}.qo" | grep -q "qore_${LIB_ID}_${LIB_ID}_script_register"
nm "${TMP}/out/${BIN_ID}.qo" | grep -q "qore_${BIN_ID}_${BIN_ID}_script_register"

# Capture --dump-info output once, then grep it.  Piping `qcc --dump-info`
# directly into `grep -q` is unsafe under `set -o pipefail`: `grep -q` exits
# as soon as it matches (the label appears early in the dump), closing the
# pipe while qcc is still writing, so qcc dies with SIGPIPE and pipefail
# fails the line even though the label is present.
DUMP_INFO="$(LD_LIBRARY_PATH=build "${QCC}" --dump-info \
    "${TMP}/out/${LIB_ID}.qo" "${TMP}/out/${BIN_ID}.qo")"
grep -q "label: ${TMP}/lib/qorus.ql" <<<"${DUMP_INFO}"
grep -q "label: ${TMP}/bin/qorus.qr" <<<"${DUMP_INFO}"

LD_LIBRARY_PATH=build "${QCC}" -o "${TMP}/app" -e verify \
    "${TMP}/out/${LIB_ID}.qo" "${TMP}/out/${BIN_ID}.qo" >/dev/null

LD_LIBRARY_PATH=build "${TMP}/app"

echo "OK: batch source identity keeps same-basename script fragments distinct"
