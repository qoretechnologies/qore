#!/bin/bash
# Regression: a source-symbol-deferred class-qualified instance method call
# (Base::method() from a subclass method) is serialized as STATIC_METHOD_CALL
# metadata until link/load time.  The AOT runtime must then resolve it with the
# same static-first, instance-fallback semantics as the parser and reconstruct a
# self-call for the instance method.

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

class BaseClient {
    *list<string> sendCmdSerialized(string cmd, hash<auto> d) {
        return (sprintf("base:%s:%y", cmd, d),);
    }
}
QORE

cat >"${TMP}/src/consumer.q" <<'QORE'
%new-style

class Client inherits BaseClient {
    *list<string> run(hash<auto> h) {
        return BaseClient::sendCmdSerialized("CMD", h);
    }

    *list<string> run_ref(hash<auto> h) {
        code<*list<string>(string, hash<auto>)> ref = \BaseClient::sendCmdSerialized();
        return ref("REF", h);
    }

    *list<string> sendCmdSerialized(string cmd, hash<auto> d) {
        throw "AOT-DISPATCH-ERROR", "explicit BaseClient::sendCmdSerialized() was dispatched to Client override";
    }
}

class ExternalCaller {
    *list<string> run(hash<auto> h) {
        return BaseClient::sendCmdSerialized("EXT", h);
    }
}

int sub main() {
    Client c();
    ExternalCaller e();
    hash<auto> h = {"a": 1};
    printf("%y|%y|%y\n", c.run(h), c.run_ref(h), e.run(h));
    return 0;
}
QORE

cat >"${TMP}/source-symbols.manifest" <<QORE
format=1
class	BaseClient	${TMP}/src/provider.q
QORE

echo "=== Step 1: compile instance-method provider ==="
"${QCC}" -c -o "${TMP}/qo/provider.qo" "${TMP}/src/provider.q" | tail -2

echo ""
echo "=== Step 2: compile subclass consumer with deferred source symbol ==="
"${QCC}" -c \
    -L "${TMP}/qo" \
    --source-symbol-manifest="${TMP}/source-symbols.manifest" \
    --write-index-json="${TMP}/consumer.idx.json" \
    -o "${TMP}/qo/consumer.qo" \
    "${TMP}/src/consumer.q" | tail -2

echo ""
echo "=== Step 3: verify deferred class-qualified call metadata ==="
grep -q 'BaseClient::sendCmdSerialized' "${TMP}/consumer.idx.json"
grep -q '"kind": "method".*"qore_path": "BaseClient::sendCmdSerialized"' "${TMP}/consumer.idx.json"
if grep -q '"kind": "function".*"qore_path": "BaseClient::sendCmdSerialized"' "${TMP}/consumer.idx.json"; then
    echo "unexpected function dependency for deferred class-qualified method call" >&2
    exit 1
fi
if grep -q 'Qore::call_static_method(string,string)' "${TMP}/consumer.idx.json"; then
    echo "unexpected dynamic static-method fallback for deferred class-qualified instance call" >&2
    exit 1
fi
strings "${TMP}/qo/consumer.qo" | grep -q '@qore-aot-static-call-arg-types-v1'
strings "${TMP}/qo/consumer.qo" | grep -q '(string,hash<auto>)'

echo ""
echo "=== Step 4: link separate .qo files and run ==="
"${QCC}" -o "${TMP}/app" "${TMP}/qo/provider.qo" "${TMP}/qo/consumer.qo" | tail -2
out="$(LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" "${TMP}/app")"
test "${out}" = '["base:CMD:{a: 1}"]|["base:REF:{a: 1}"]|["base:EXT:{a: 1}"]'
printf '%s\n' "${out}"

echo ""
echo "OK: deferred class-qualified instance method call linked and executed."
