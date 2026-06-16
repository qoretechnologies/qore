#!/bin/bash
# Regression: a source-symbol-deferred typed object construction such as
# `Tx trans(args)` serializes a deferred NEW_OBJECT slot.  Runtime slot
# reconstruction must resolve encoded AOT class refs such as `::Tx` instead of
# treating them as unresolved pending source links.

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

cat >"${TMP}/src/provider.q" <<'QORE'
%new-style

class Tx {
    constructor(string driver) {
    }

    string get() {
        return "tx";
    }
}
QORE

cat >"${TMP}/src/consumer.q" <<'QORE'
%new-style

string sub getDriverName() {
    return "pgsql";
}

int sub main() {
    Tx trans(getDriverName());
    printf("%s\n", trans.get());
    return 0;
}
QORE

cat >"${TMP}/source-symbols.manifest" <<QORE
format=1
class	Tx	${TMP}/src/provider.q
QORE

echo "=== Step 1: compile constructor provider ==="
"${QCC}" -c -o "${TMP}/qo/provider.qo" "${TMP}/src/provider.q" | tail -2

echo ""
echo "=== Step 2: compile consumer with deferred source class ==="
"${QCC}" -c \
    -L "${TMP}/qo" \
    --source-symbol-manifest="${TMP}/source-symbols.manifest" \
    --write-index-json="${TMP}/consumer.idx.json" \
    -o "${TMP}/qo/consumer.qo" \
    "${TMP}/src/consumer.q" | tail -2

echo ""
echo "=== Step 3: verify deferred constructor metadata ==="
strings "${TMP}/qo/consumer.qo" | grep -q 'deferred-create-object'
strings "${TMP}/qo/consumer.qo" | grep -q 'object<::Tx>'

echo ""
echo "=== Step 4: link separate .qo files and run ==="
"${QCC}" -o "${TMP}/app" "${TMP}/qo/provider.qo" "${TMP}/qo/consumer.qo" | tail -2
out="$(LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" "${TMP}/app")"
test "${out}" = 'tx'
printf '%s\n' "${out}"

echo ""
echo "OK: deferred object construction linked and executed."
