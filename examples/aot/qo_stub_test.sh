#!/bin/bash
# AOT Phase 4 slice 11b end-to-end harness for `qcc --stub=<file>`.
#
# A stub file declares a namespace, constants, and functions that the
# host will synthesize in C++ at runtime.  The target source
# references those names by bare identifier; without the stub the
# parse fails, with the stub it compiles and the target's .qo is
# emitted as usual (no .qo for the stub).
#
# Run from the qore repo root:
#   LD_LIBRARY_PATH=build ./examples/aot/qo_stub_test.sh

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

QCC="./build/qcc"

source_id() {
    realpath "$1" | sed 's/[^A-Za-z0-9_]/_/g'
}

cat > "${TMP}/host_stub.qc" <<'EOF'
# Declarative stubs — parsed into compile program, no .qo emitted.
%modern

public namespace MyHost {
    public const HostVersion = "1.0";
    public const HostLimit = 42;
    public int sub host_get_limit() {
        return 0;
    }
}
EOF

cat > "${TMP}/target.qc" <<'EOF'
%modern

int sub compute() {
    # Cross-file references to the stub-declared symbols.
    return MyHost::HostLimit + MyHost::host_get_limit();
}
EOF

echo "=== Step 1: negative control — no stub, should fail ==="
if "${QCC}" -c -o "${TMP}/target_nostub.qo" "${TMP}/target.qc" >/dev/null 2>&1; then
    echo "FAIL: parse succeeded without stub (expected fail)"
    exit 1
fi
echo "  confirmed: compile without --stub fails as expected"

echo ""
echo "=== Step 2: with --stub, target compiles cleanly ==="
"${QCC}" -c --stub="${TMP}/host_stub.qc" \
    -o "${TMP}/target.qo" "${TMP}/target.qc" | tail -4

echo ""
echo "=== Step 3: output inventory ==="
echo "files in ${TMP}:"
ls -la "${TMP}" | tail -n +2
# Stub must not produce a .qo.
if [ -f "${TMP}/host_stub.qo" ]; then
    echo "FAIL: stub emitted a .qo (expected none)"
    exit 1
fi
# Target must have its register entry point.
SYMS=$(nm "${TMP}/target.qo" | grep -cE "T qore_target_target_script_register" || true)
if [ "${SYMS}" != "1" ]; then
    echo "FAIL: expected 1 register entry point, got ${SYMS}"
    exit 1
fi
echo "  confirmed: stub produced no .qo; target has 1 register entry point"

echo ""
echo "=== Step 4: batch mode with --stub ==="
cat > "${TMP}/target2.qc" <<'EOF'
%modern

string sub greet() {
    return "host=" + MyHost::HostVersion;
}
EOF
BATCH_DIR="${TMP}/batch"
mkdir -p "${BATCH_DIR}"
TARGET_ID="$(source_id "${TMP}/target.qc")"
TARGET2_ID="$(source_id "${TMP}/target2.qc")"
"${QCC}" -c --stub="${TMP}/host_stub.qc" \
    --output-dir="${BATCH_DIR}" \
    "${TMP}/target.qc" "${TMP}/target2.qc" | tail -2
# Both targets must have .qo; stub must not.
[ -f "${BATCH_DIR}/${TARGET_ID}.qo" ] || { echo "FAIL: ${TARGET_ID}.qo missing"; exit 1; }
[ -f "${BATCH_DIR}/${TARGET2_ID}.qo" ] || { echo "FAIL: ${TARGET2_ID}.qo missing"; exit 1; }
if find "${BATCH_DIR}" -maxdepth 1 -name '*host_stub*.qo' | grep -q .; then
    echo "FAIL: stub emitted a .qo"
    exit 1
fi
echo "  confirmed: batch mode emitted 2 target .qos, no stub .qo"

echo ""
echo "OK: slice 11b --stub file preload works (single-file + batch,"
echo "    stub decls visible to parser, no .qo emitted for stubs)."
