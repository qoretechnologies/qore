#!/bin/bash
# Regression: a class member default in a preloaded sibling .qo may call a
# source-deferred static method whose provider .qo has not been built yet.
# Deserializing the member default must preserve the unresolved static call
# instead of failing during sibling cross-resolution.

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
%strict-args
%require-types

class MemberDefaultProvider {
    public static string value() {
        return "linked-member-default";
    }
}
QORE

cat >"${TMP}/src/holder.q" <<'QORE'
%new-style
%strict-args
%require-types

class MemberDefaultHolder {
    private {
        string value = MemberDefaultProvider::value();
    }

    string get() {
        return value;
    }
}
QORE

cat >"${TMP}/src/user.q" <<'QORE'
%new-style
%strict-args
%require-types

int sub main() {
    MemberDefaultHolder holder();
    printf("%s\n", holder.get());
    return 0;
}
QORE

cat >"${TMP}/source-symbols.manifest" <<QORE
format=1
class	MemberDefaultProvider	${TMP}/src/provider.q
QORE

echo "=== Step 1: compile member-default holder before provider ==="
"${QCC}" -c \
    --source-symbol-manifest="${TMP}/source-symbols.manifest" \
    --write-index-json="${TMP}/holder.idx.json" \
    -o "${TMP}/qo/holder.qo" \
    "${TMP}/src/holder.q" | tail -2

echo ""
echo "=== Step 2: compile holder user with holder.qo preloaded and provider absent ==="
"${QCC}" -c \
    -L "${TMP}/qo" \
    --source-symbol-manifest="${TMP}/source-symbols.manifest" \
    --write-index-json="${TMP}/user.idx.json" \
    -o "${TMP}/qo/user.qo" \
    "${TMP}/src/user.q" | tail -2

echo ""
echo "=== Step 3: verify deferred member-default metadata ==="
grep -q 'MemberDefaultProvider::value' "${TMP}/holder.idx.json"

echo ""
echo "=== Step 4: compile provider, link, and run ==="
"${QCC}" -c -o "${TMP}/qo/provider.qo" "${TMP}/src/provider.q" | tail -2
"${QCC}" -o "${TMP}/app" "${TMP}/qo/provider.qo" "${TMP}/qo/holder.qo" "${TMP}/qo/user.qo" | tail -2

out="$(LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" "${TMP}/app")"
test "${out}" = "linked-member-default"
printf '%s\n' "${out}"

echo ""
echo "OK: source-deferred static method member default linked and executed."
