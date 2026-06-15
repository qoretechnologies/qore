#!/bin/bash
# Regression: a constant initializer may call a source-deferred static method.
# The compile-time constant evaluation must defer to the linked .qo instead of
# failing with METHOD-CALL-ERROR while the provider source object is not built.

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

QCC="${QCC:-./build/qcc}"
mkdir -p "${TMP}/src" "${TMP}/qo"

cat >"${TMP}/src/provider.q" <<'QORE'
%new-style
%strict-args
%require-types

class ConstProvider {
    public static string value() {
        return "linked-const";
    }
}
QORE

cat >"${TMP}/src/consumer.q" <<'QORE'
%new-style
%strict-args
%require-types

class ConstConsumer {
    public {
        const Value = ConstProvider::value();
    }

    static string get() {
        return Value;
    }
}

int sub main() {
    printf("%s\n", ConstConsumer::get());
    return 0;
}
QORE

cat >"${TMP}/source-symbols.manifest" <<QORE
format=1
class	ConstProvider	${TMP}/src/provider.q
class	ConstConsumer	${TMP}/src/consumer.q
QORE

echo "=== Step 1: compile constant consumer before provider ==="
"${QCC}" -c \
    --source-symbol-manifest="${TMP}/source-symbols.manifest" \
    --write-index-json="${TMP}/consumer.idx.json" \
    -o "${TMP}/qo/consumer.qo" \
    "${TMP}/src/consumer.q" | tail -2

echo ""
echo "=== Step 2: verify deferred static-method metadata ==="
grep -q 'ConstProvider::value' "${TMP}/consumer.idx.json"

echo ""
echo "=== Step 3: compile provider, link, and run ==="
"${QCC}" -c -o "${TMP}/qo/provider.qo" "${TMP}/src/provider.q" | tail -2
"${QCC}" -o "${TMP}/app" "${TMP}/qo/provider.qo" "${TMP}/qo/consumer.qo" | tail -2

out="$(LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" "${TMP}/app")"
test "${out}" = "linked-const"
printf '%s\n' "${out}"

echo ""
echo "OK: source-deferred static method constant initializer linked and executed."
