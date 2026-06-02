#!/bin/bash
# AOT Phase 4 slice 6: end-to-end test for `qcc -m --from-objects`.
#
# Builds per-file `.qo`s from qlib/AsyncSocketIo, aggregates them into
# a `.qmod`, loads the `.qmod` via QORE_MODULE_DIR, and exercises a
# class defined in a secondary `.qc` — proving that the aggregator
# stitches primary + secondaries into a runnable module.
#
# Run from the qore repo root:
#   LD_LIBRARY_PATH=build ./examples/aot/qo_aggregator_test.sh

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

SRC_DIR="qlib/AsyncSocketIo"
QCC="./build/qcc"

echo "=== Step 1: per-file .qo builds ==="
"${QCC}" -c --context="${SRC_DIR}" \
    -o "${TMP}/AsyncSocketIo_primary.qo" "${SRC_DIR}/AsyncSocketIo.qm" | tail -1
for f in "${SRC_DIR}"/*.qc; do
    out="${TMP}/$(basename "${f}" .qc).qo"
    "${QCC}" -c --context="${SRC_DIR}" -o "${out}" "${f}" | tail -1
done

echo ""
echo "=== Step 2: aggregate into .qmod ==="
"${QCC}" -m --from-objects --context="${SRC_DIR}" "${TMP}"/*.qo \
    -o "${TMP}/AsyncSocketIo.qmod" | tail -3

echo ""
echo "=== Step 3: load + exercise ==="
mkdir -p "${TMP}/mod"
cp "${TMP}/AsyncSocketIo.qmod" "${TMP}/mod/"

cat > "${TMP}/drive.q" <<'EOF'
%new-style
%requires AsyncSocketIo
printf("aggregated AsyncSocketIo.qmod: loaded\n");
AsyncSocketIo::AsyncSocketIoController ctl();
printf("aggregated AsyncSocketIo.qmod: controller constructed (class from .qc)\n");
ctl.stop();
printf("aggregated AsyncSocketIo.qmod: ctl.stop() returned (method from .qc)\n");
EOF

QORE_MODULE_DIR="${TMP}/mod:build/modules/reflection:qlib:examples/test/qlib" \
    ./build/qore "${TMP}/drive.q"

echo ""
echo "OK: aggregator produced a loadable .qmod exercising cross-.qo symbols."
