#!/bin/sh
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Reproduces, with no Qore code, the report of LLVM 21.1 on aarch64 that the suppression
# llvm21-instcombine-select-binop-arm64-csel in examples/test/valgrind.supp covers (qore#7729): InstCombine's
# InstCombinerImpl::SimplifySelectsFeedingBinaryOp() reads an uninitialised local for a binary operator whose first
# operand is a select and whose second is not (see the comment of the suppression).  Each of the two runs below reports
# "Conditional jump or move depends on uninitialised value(s)" in five unnamed frames of libLLVM.so.21.1 under
# llvm::InstCombinePass::run(), with the uninitialised value "created by a stack allocation" in the innermost of them:
#   1. LLVM's own opt on f.ll
#   2. instcombine_select_and.cpp, which makes the calls of Qore's optimizeModule() (lib/QoreJIT.cpp) on the same
#      function, built in a temporary directory
# Usage: run.sh [<llvm-config>] (default: llvm-config-21); it needs valgrind and a C++ compiler.  The runs are not
# suppressed: valgrind gets no suppressions file (VALGRIND_OPTS is cleared).

set -e
dir=$(cd "$(dirname "$0")" && pwd)
llvm_config=${1:-llvm-config-21}
bindir=$("$llvm_config" --bindir)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM

echo "== opt: $bindir/opt -passes='instcombine<no-verify-fixpoint>' f.ll"
VALGRIND_OPTS= valgrind --track-origins=yes "$bindir/opt" -passes='instcombine<no-verify-fixpoint>' -disable-output \
    "$dir/f.ll"

echo "== instcombine_select_and.cpp (the calls of optimizeModule())"
c++ -std=c++17 "$dir/instcombine_select_and.cpp" $("$llvm_config" --cxxflags --ldflags --libs) \
    -o "$tmp/instcombine_select_and"
VALGRIND_OPTS= valgrind --track-origins=yes "$tmp/instcombine_select_and" > /dev/null
