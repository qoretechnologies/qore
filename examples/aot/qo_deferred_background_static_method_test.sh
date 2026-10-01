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
%modern

class BackgroundProvider {
    public static write(string path, string text, Counter done) {
        on_exit done.dec();
        File f();
        f.open2(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
        f.write(text);
        f.close();
    }
}
QORE

cat >"${TMP}/src/consumer.q" <<QORE
%modern

class BackgroundConsumer {
    static start(string path, Counter done) {
        done.inc();
        background BackgroundProvider::write(path, "linked-background-static", done);
    }
}

int sub main() {
    string path = "${TMP}/background.out";
    # the background call decrements the counter only after it has written and closed the file
    Counter done();
    BackgroundConsumer::start(path, done);
    if (done.waitForZero(20s)) {
        throw "BACKGROUND-STATIC-TEST", "background static method did not complete";
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
