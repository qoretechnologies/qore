#!/bin/bash
# Regression: a source-symbol-deferred static method call must stay a
# STATIC_METHOD_CALL through AOT/linking.  Lowering it to Qore::call_static_method()
# loses typed hashdecl argument metadata and can make overload resolution see a
# plain hash at runtime.

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

hashdecl AutoAssignNodeOptionInfo {
    bool no_exit_on_error;
}

class QorusCommonLib {
    public static string autoAssignNode(reference<hash<string, bool>> local_addresses,
            *hash<AutoAssignNodeOptionInfo> options) {
        local_addresses{"provider"} = True;
        return options.no_exit_on_error ? "no-exit" : "exit";
    }
}
QORE

cat >"${TMP}/src/consumer.q" <<'QORE'
%new-style

class ClientProcessBase {
    static string run(bool exit_on_err) {
        hash<string, bool> local_addresses = {};
        string rv = QorusCommonLib::autoAssignNode(\local_addresses,
            <AutoAssignNodeOptionInfo>{"no_exit_on_error": !exit_on_err});
        return sprintf("%s:%y", rv, local_addresses);
    }
}

int sub main() {
    printf("%s\n", ClientProcessBase::run(False));
    return 0;
}
QORE

cat >"${TMP}/source-symbols.manifest" <<QORE
format=1
class	QorusCommonLib	${TMP}/src/provider.q
hashdecl	AutoAssignNodeOptionInfo	${TMP}/src/provider.q
QORE

echo "=== Step 1: compile static-method provider ==="
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
grep -q 'QorusCommonLib::autoAssignNode' "${TMP}/consumer.idx.json"
if grep -q 'Qore::call_static_method(string,string)' "${TMP}/consumer.idx.json"; then
    echo "unexpected dynamic static-method fallback for deferred static call" >&2
    exit 1
fi

echo ""
echo "=== Step 4: link separate .qo files and run ==="
"${QCC}" -o "${TMP}/app" "${TMP}/qo/provider.qo" "${TMP}/qo/consumer.qo" | tail -2
out="$(LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" "${TMP}/app")"
test "${out}" = 'no-exit:{provider: True}'
printf '%s\n' "${out}"

echo ""
echo "OK: deferred static method call with typed hashdecl args linked and executed."
