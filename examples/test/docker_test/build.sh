#!/bin/sh

set -e
set -x

# Build script - compiles Qore and produces build artifacts.
# Used by the CI build stage; test jobs download the build/ artifact
# and skip compilation.

# Source environment from the Docker image (provides INSTALL_PREFIX, etc.)
ENV_FILE=/tmp/env.sh
if [ -f "${ENV_FILE}" ]; then
    . ${ENV_FILE}
fi

# setup QORE_SRC_DIR env var
cwd=`pwd`
if [ "${QORE_SRC_DIR}" = "" ]; then
    if [ -e "$cwd/qlib/SqlUtil.qm" ] || [ -e "$cwd/bin/qdbg" ] || [ -e "$cwd/cmake/QoreMacros.cmake" ] || [ -e "$cwd/lib/QoreLib.cpp" ]; then
        QORE_SRC_DIR=$cwd
    else
        QORE_SRC_DIR=$WORKDIR/qore
    fi
fi

export MAKE_JOBS=${MAKE_JOBS:-6}

# install tree-sitter CLI for astparser module build
if ! command -v tree-sitter > /dev/null 2>&1; then
    echo && echo "-- installing tree-sitter CLI --"
    cargo install tree-sitter-cli@0.26.5
fi

# build Qore
echo && echo "-- building Qore --"
cd ${QORE_SRC_DIR}
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-debug} -DSINGLE_COMPILATION_UNIT=1 -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}
make -j${MAKE_JOBS}

# Remove intermediate object files to reduce artifact size
find . -name "*.o" -delete 2>/dev/null || true
