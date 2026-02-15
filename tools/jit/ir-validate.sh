#!/bin/sh

set -e

QORE_BIN="${QORE_BIN:-./build-debug/qore}"
BENCH=0

if [ "${1:-}" = "--bench" ]; then
  BENCH=1
  shift
fi

TEST="${1:-./examples/test/ir/IRExecModeSmoke.qtest}"

echo "Validating AST vs IR: ${TEST}"
echo "AST: ${QORE_BIN} ${TEST}"
"${QORE_BIN}" "${TEST}"
echo "IR:  ${QORE_BIN} --exec-mode=ir ${TEST}"
"${QORE_BIN}" --exec-mode=ir "${TEST}"
echo "OK"

if [ "${BENCH}" -eq 1 ]; then
  BENCH_TEST="${BENCH_TEST:-./tools/jit/bench/loop.q}"
  TIME_BIN="${TIME_BIN:-/usr/bin/time}"
  if [ ! -x "${TIME_BIN}" ]; then
    TIME_BIN="time"
  fi
  echo "Benchmarking (AST/IR/JIT): ${BENCH_TEST}"
  for mode in ast ir jit; do
    echo "BENCH ${mode}: ${QORE_BIN} --exec-mode=${mode} ${BENCH_TEST}"
    err_file="$(mktemp)"
    if ! ${TIME_BIN} -p "${QORE_BIN}" --exec-mode="${mode}" "${BENCH_TEST}" > /dev/null 2> "${err_file}"; then
      if grep -q "invalid --exec-mode" "${err_file}"; then
        echo "SKIP ${mode}: exec-mode not supported by this build"
        rm -f "${err_file}"
        continue
      fi
      cat "${err_file}"
      rm -f "${err_file}"
      exit 1
    fi
    rm -f "${err_file}"
  done
  echo "BENCH OK"
fi
