#!/bin/bash
# Regression: a background static method call may target a source-deferred
# provider class.  AOT lowering must serialize native background metadata for
# the qualified static call instead of requiring an expression-tree fallback.

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

class BackgroundProvider {
    public static write(string path, string text) {
        File f();
        f.open2(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
        f.write(text);
        f.close();
    }
}
QORE

cat >"${TMP}/src/consumer.q" <<QORE
%new-style
%strict-args
%require-types

class BackgroundConsumer {
    static start(string path) {
        background BackgroundProvider::write(path, "linked-background-static");
    }
}

int sub main() {
    string path = "${TMP}/background.out";
    BackgroundConsumer::start(path);

    bool ready = False;
    for (int i = 0; i < 200; ++i) {
        *hash<StatInfo> info = hstat(path);
        if (exists info) {
            ready = True;
            break;
        }
        usleep(10000);
    }
    if (!ready) {
        throw "BACKGROUND-STATIC-TEST", "background static method did not write output";
    }

    string out = ReadOnlyFile::readTextFile(path);
    if (out != "linked-background-static") {
        throw "BACKGROUND-STATIC-TEST", sprintf("unexpected output: '%s'", out);
    }

    printf("%s\n", out);
    return 0;
}
QORE

cat >"${TMP}/source-symbols.manifest" <<QORE
format=1
class	BackgroundProvider	${TMP}/src/provider.q
QORE

echo "=== Step 1: compile background consumer before provider ==="
"${QCC}" -c \
    --source-symbol-manifest="${TMP}/source-symbols.manifest" \
    --write-index-json="${TMP}/consumer.idx.json" \
    -o "${TMP}/qo/consumer.qo" \
    "${TMP}/src/consumer.q" | tail -2

echo ""
echo "=== Step 2: verify deferred background static-method metadata ==="
grep -q 'BackgroundProvider::write' "${TMP}/consumer.idx.json"

echo ""
echo "=== Step 3: compile provider, link, and run ==="
"${QCC}" -c -o "${TMP}/qo/provider.qo" "${TMP}/src/provider.q" | tail -2
"${QCC}" -o "${TMP}/app" "${TMP}/qo/provider.qo" "${TMP}/qo/consumer.qo" | tail -2

out="$(LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" "${TMP}/app")"
test "${out}" = "linked-background-static"
printf '%s\n' "${out}"

echo ""
echo "OK: source-deferred background static method linked and executed."
