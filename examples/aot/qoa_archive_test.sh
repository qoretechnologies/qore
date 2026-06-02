#!/bin/bash
# AOT Phase 4 slice 7: end-to-end test for `qcc -a`.
#
# Builds per-file `.qo`s for qlib/AsyncSocketIo, archives them into a
# `.qoa` static library, links the archive into a small C++ host, and
# runs the host — proving that `qore_qoa_register_all(pgm)` registers
# the module's contents from a fully static link (no dlopen, no
# on-disk `.qm`/`.qmod`).
#
# Run from the qore repo root:
#   LD_LIBRARY_PATH=build ./examples/aot/qoa_archive_test.sh

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
echo "=== Step 2: archive into .qoa ==="
"${QCC}" -a --context="${SRC_DIR}" "${TMP}"/*.qo \
    -o "${TMP}/AsyncSocketIo.qoa" | tail -3

echo ""
echo "=== Step 3: archive contents ==="
ar t "${TMP}/AsyncSocketIo.qoa"
echo ""
echo "--- key symbols ---"
nm "${TMP}/AsyncSocketIo.qoa" | grep -E "T (qore_qoa_register_all|qore_AsyncSocketIo_register|AsyncSocketIo_qore_module_desc)" || true

echo ""
echo "=== Step 4: link into C++ host + run ==="
g++ -std=c++17 -Iinclude -Ibuild -Ibuild/include -Lbuild \
    examples/aot/qoa_link_test.cpp \
    "${TMP}/AsyncSocketIo.qoa" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/qoa_link_test"

LD_LIBRARY_PATH=build "${TMP}/qoa_link_test"

echo ""
echo "OK: .qoa archive statically linked + qore_qoa_register_all() "
echo "    exercised a secondary-.qc class method."
