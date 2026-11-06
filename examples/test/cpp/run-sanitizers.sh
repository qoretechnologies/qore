#!/bin/sh
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Builds the interpreter-free core of the HTTP server fast routes (lib/HttpFastRoutes.cpp, lib/HttpFastRequest.cpp) with its standalone test
# (test/cpp/http_fast_routes_core_test.cpp) under AddressSanitizer + UndefinedBehaviorSanitizer and under
# ThreadSanitizer, and runs both.  The core depends only on the standard library and qore/common.h, so no Qore build is
# needed.  See design/http-server-fast-routes.md.
#
# usage: test/cpp/run-sanitizers.sh [build-dir]   (build-dir defaults to a temporary directory)

set -e

src=$(cd "$(dirname "$0")/../.." && pwd)
out=${1:-$(mktemp -d)}
mkdir -p "$out"
cxx=${CXX:-c++}

# qore/common.h is in include/; the generated headers of an existing build (if any) are used when present
inc="-I$src/include"
if [ -d "$src/build/include" ]; then
    inc="$inc -I$src/build/include"
fi

build_and_run() {
    name=$1
    shift
    echo "-- $name --"
    $cxx -std=c++17 -g -O1 -fno-omit-frame-pointer "$@" $inc \
        "$src/lib/HttpFastRoutes.cpp" "$src/lib/HttpFastRequest.cpp" "$src/test/cpp/http_fast_routes_core_test.cpp" -o "$out/core-$name" -lpthread
    "$out/core-$name"
}

build_and_run asan-ubsan -fsanitize=address,undefined -fno-sanitize-recover=undefined
build_and_run tsan -fsanitize=thread
