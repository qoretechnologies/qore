#!/bin/bash
# Regression: a class-qualified call shape in a closure initializer can still be
# a namespace function after method lookup fails.  AOT deserialization must match
# StaticMethodCallNode source parsing and resolve the namespace function instead
# of requiring a class method.

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

public namespace OMQ {
    public namespace UserApi {
        public string sub helper(string s) {
            return "ok:" + s;
        }

        public class UserApi {
        }
    }
}
QORE

cat >"${TMP}/src/consumer.q" <<'QORE'
%new-style
%strict-args
%require-types

public namespace OMQ {
    class Consumer {
        public {
            const Handler = string sub(string s) {
                return UserApi::helper(s);
            };
        }

        public static string run(string s) {
            return Handler(s);
        }
    }
}

int sub main() {
    printf("%s\n", OMQ::Consumer::run("x"));
    return 0;
}
QORE

cat >"${TMP}/source-symbols.manifest" <<QORE
format=1
class	UserApi	${TMP}/src/provider.q
class	OMQ::UserApi::UserApi	${TMP}/src/provider.q
function	UserApi::helper	${TMP}/src/provider.q
function	OMQ::UserApi::helper	${TMP}/src/provider.q
QORE

echo "=== Step 1: compile consumer before provider ==="
"${QCC}" -c \
    --source-symbol-manifest="${TMP}/source-symbols.manifest" \
    --write-index-json="${TMP}/consumer.idx.json" \
    -o "${TMP}/qo/consumer.qo" \
    "${TMP}/src/consumer.q" | tail -2

echo ""
echo "=== Step 2: verify namespace function dependency metadata ==="
grep -q '"kind": "function".*"qore_path": "UserApi::helper"' "${TMP}/consumer.idx.json"

echo ""
echo "=== Step 3: compile provider, link separate .qo files, and run ==="
"${QCC}" -c -o "${TMP}/qo/provider.qo" "${TMP}/src/provider.q" | tail -2
"${QCC}" -o "${TMP}/app" "${TMP}/qo/provider.qo" "${TMP}/qo/consumer.qo" | tail -2

out="$(LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" "${TMP}/app")"
test "${out}" = "ok:x"
printf '%s\n' "${out}"

echo ""
echo "OK: class-qualified namespace function fallback in closure initializer linked and executed."
