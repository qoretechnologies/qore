#!/bin/bash
# Regression: source-symbol manifests are generated from source declarations and
# record classes by their declaration name.  A qualified receiver such as
# OMQ::MapperProgram must still defer to the source provider when that simple
# name has one unambiguous provider in the manifest.

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
    public class MapperProgram {
        public static string sanitizeFieldName(string name) {
            return "sanitized:" + name;
        }

        public static hash<string, string> getTypeMap() {
            return {
                "qore": "string",
            };
        }
    }
}
QORE

cat >"${TMP}/src/consumer.q" <<'QORE'
%new-style
%strict-args
%require-types

namespace OMQ {
    class MapperFieldCodeTypeHelper {
        static string field(string name) {
            return OMQ::MapperProgram::sanitizeFieldName(name);
        }

        static hash<string, string> typeMap() {
            return OMQ::MapperProgram::getTypeMap();
        }
    }
}

int sub main() {
    printf("%s:%s\n", OMQ::MapperFieldCodeTypeHelper::field("part_id"),
        OMQ::MapperFieldCodeTypeHelper::typeMap().qore);
    return 0;
}
QORE

cat >"${TMP}/source-symbols.manifest" <<QORE
format=1
class	MapperProgram	${TMP}/src/provider.q
class	MapperFieldCodeTypeHelper	${TMP}/src/consumer.q
QORE

echo "=== Step 1: compile consumer before provider ==="
"${QCC}" -c \
    --source-symbol-manifest="${TMP}/source-symbols.manifest" \
    --write-index-json="${TMP}/consumer.idx.json" \
    -o "${TMP}/qo/consumer.qo" \
    "${TMP}/src/consumer.q" | tail -2

echo ""
echo "=== Step 2: verify deferred static-method metadata ==="
grep -q 'OMQ::MapperProgram::sanitizeFieldName' "${TMP}/consumer.idx.json"
grep -q 'OMQ::MapperProgram::getTypeMap' "${TMP}/consumer.idx.json"
if grep -q 'Qore::call_static_method(string,string)' "${TMP}/consumer.idx.json"; then
    echo "unexpected dynamic static-method fallback for namespaced source-deferred class" >&2
    exit 1
fi

echo ""
echo "=== Step 3: compile provider and link ==="
"${QCC}" -c -o "${TMP}/qo/provider.qo" "${TMP}/src/provider.q" | tail -2
"${QCC}" -o "${TMP}/app" "${TMP}/qo/provider.qo" "${TMP}/qo/consumer.qo" | tail -2

out="$(LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" "${TMP}/app")"
test "${out}" = "sanitized:part_id:string"
printf '%s\n' "${out}"

echo ""
echo "OK: deferred namespaced class static methods linked and executed."
