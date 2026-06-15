#!/bin/bash
# Regression: a source-symbol-deferred namespace-qualified function call or call
# reference (Ns::fn() / \Ns::fn()) must use function resolution, not the class
# static-method fallback.  Otherwise a separately compiled consumer can fail at
# runtime with UNKNOWN-CLASS for the namespace receiver.

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

QCC="${QCC:-./build/qcc}"
mkdir -p "${TMP}/src" "${TMP}/qo"

cat >"${TMP}/src/provider.q" <<'QORE'
%new-style

public namespace OMQ {
    public string sub get_option_name(string domain, string name) {
        return domain + ":" + name;
    }
}
QORE

cat >"${TMP}/src/consumer.q" <<'QORE'
%new-style

class Consumer {
    static string run(string value) {
        return OMQ::get_option_name("qorus", value);
    }

    static string run_ref(string value) {
        code<string(string, string)> ref = \OMQ::get_option_name();
        return ref("qorus", value);
    }
}

int sub main() {
    printf("%s|%s\n", Consumer::run("http-server"), Consumer::run_ref("callref"));
    return 0;
}
QORE

cat >"${TMP}/source-symbols.manifest" <<QORE
format=1
class	OMQ	${TMP}/src/provider.q
function	OMQ::get_option_name	${TMP}/src/provider.q
function	get_option_name	${TMP}/src/provider.q
QORE

echo "=== Step 1: compile namespace-function provider ==="
"${QCC}" -c -o "${TMP}/qo/provider.qo" "${TMP}/src/provider.q" | tail -2

echo ""
echo "=== Step 2: compile consumer without provider preload ==="
"${QCC}" -c \
    --source-symbol-manifest="${TMP}/source-symbols.manifest" \
    --write-index-json="${TMP}/consumer.idx.json" \
    -o "${TMP}/qo/consumer.qo" \
    "${TMP}/src/consumer.q" | tail -2

echo ""
echo "=== Step 3: verify deferred fallback kind ==="
grep -q 'Qore::call_function(string)' "${TMP}/consumer.idx.json"
grep -q '"kind": "function".*"qore_path": "OMQ::get_option_name"' "${TMP}/consumer.idx.json"
if grep -q 'Qore::call_static_method(string,string)' "${TMP}/consumer.idx.json"; then
    echo "unexpected static-method fallback for namespace function" >&2
    exit 1
fi
if grep -q '"kind": "static_method".*"qore_path": "OMQ::get_option_name"' "${TMP}/consumer.idx.json"; then
    echo "unexpected static-method dependency for namespace function call reference" >&2
    exit 1
fi

echo ""
echo "=== Step 4: link separate .qo files and run ==="
"${QCC}" -o "${TMP}/app" "${TMP}/qo/provider.qo" "${TMP}/qo/consumer.qo" | tail -2
out="$(LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" "${TMP}/app")"
test "${out}" = "qorus:http-server|qorus:callref"
printf '%s\n' "${out}"

echo ""
echo "OK: deferred namespace-qualified function call and call reference linked and executed."
