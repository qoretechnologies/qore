#!/bin/sh

set -e

QORE_BIN="${QORE_BIN:-./build-debug/qore}"
TEST="${1:-./examples/test/ir/IRExecModeSmoke.qtest}"

echo "Validating AST vs IR: ${TEST}"
echo "AST: ${QORE_BIN} ${TEST}"
"${QORE_BIN}" "${TEST}"
echo "IR:  ${QORE_BIN} --exec-mode=ir ${TEST}"
"${QORE_BIN}" --exec-mode=ir "${TEST}"
echo "OK"
