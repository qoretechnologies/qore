#!/bin/sh

set -e
set -x

# Performance test script - builds with Release optimization and runs only perf tests.
# No database services are required.

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

# build Qore with Release optimization
echo && echo "-- building Qore (Release) --"
cd ${QORE_SRC_DIR}
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DSINGLE_COMPILATION_UNIT=1 -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}
make -j${MAKE_JOBS}
make install

# run only performance tests
export QORE_MODULE_DIR=${QORE_SRC_DIR}/qlib:${QORE_MODULE_DIR}
cd ${QORE_SRC_DIR}
./run_tests.sh -P
