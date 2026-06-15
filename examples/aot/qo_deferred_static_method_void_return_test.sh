#!/bin/bash
# Regression: a no-return wrapper may use `return Provider::method();` where
# the provider class is source-deferred.  The parser must not reject the wrapper
# before link-time can resolve that the static method returns no value.

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

class ServiceApi {
    public static stopListenerId(int id) {
    }
}
QORE

cat >"${TMP}/src/consumer.q" <<'QORE'
%new-style
%strict-args
%require-types

sub svc_stop_listener_id(int id) {
    return ServiceApi::stopListenerId(id);
}

int sub main() {
    svc_stop_listener_id(7);
    printf("ok\n");
    return 0;
}
QORE

cat >"${TMP}/source-symbols.manifest" <<QORE
format=1
class	ServiceApi	${TMP}/src/provider.q
function	svc_stop_listener_id	${TMP}/src/consumer.q
QORE

echo "=== Step 1: compile no-return wrapper before provider ==="
"${QCC}" -c \
    --source-symbol-manifest="${TMP}/source-symbols.manifest" \
    --write-index-json="${TMP}/consumer.idx.json" \
    -o "${TMP}/qo/consumer.qo" \
    "${TMP}/src/consumer.q" | tail -2

echo ""
echo "=== Step 2: verify deferred static-method metadata ==="
grep -q 'ServiceApi::stopListenerId' "${TMP}/consumer.idx.json"
if grep -q 'Qore::call_static_method(string,string)' "${TMP}/consumer.idx.json"; then
    echo "unexpected dynamic static-method fallback for deferred void return" >&2
    exit 1
fi

echo ""
echo "=== Step 3: compile provider and link ==="
"${QCC}" -c -o "${TMP}/qo/provider.qo" "${TMP}/src/provider.q" | tail -2
"${QCC}" -o "${TMP}/app" "${TMP}/qo/provider.qo" "${TMP}/qo/consumer.qo" | tail -2

out="$(LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" "${TMP}/app")"
test "${out}" = "ok"
printf '%s\n' "${out}"

echo ""
echo "OK: source-deferred static method return in no-return wrapper linked and executed."
