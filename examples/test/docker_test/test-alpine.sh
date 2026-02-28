#!/bin/bash

set -e
set -x

ENV_FILE=/tmp/env.sh

# setup QORE_SRC_DIR env var
cwd=`pwd`
if [ "${QORE_SRC_DIR}" = "" ]; then
    if [ -e "$cwd/qlib/SqlUtil.qm" ] || [ -e "$cwd/bin/qdbg" ] || [ -e "$cwd/cmake/QoreMacros.cmake" ] || [ -e "$cwd/lib/QoreLib.cpp" ]; then
        QORE_SRC_DIR=$cwd
    else
        QORE_SRC_DIR=$WORKDIR/qore
    fi
fi

echo "export QORE_SRC_DIR=${QORE_SRC_DIR}" >> ${ENV_FILE}

echo "export QORE_UID=1000" >> ${ENV_FILE}
echo "export QORE_GID=1000" >> ${ENV_FILE}

. ${ENV_FILE}

if [ -z "${QORE_DB_CONNSTR_PGSQL}" ]; then
    apk add --no-cache postgresql-client
    . examples/test/docker_test/postgres_lib.sh
    setup_postgres_on_host
    drop_pgsql_schema=1
fi
if [ -z "${QORE_DB_CONNSTR_ORACLE}" ]; then
    . examples/test/docker_test/init_oracle.sh
    . ${ENV_FILE}
fi
if [ -z "${REDIS_URL}" ]; then
    . examples/test/docker_test/redis_lib.sh
    setup_redis_on_host
fi

find / -name "libqore.so*" -exec rm -f {} \;

# ensure LLVM static libraries are installed (needed for JIT/AOT linking)
# Alpine splits static libs into -static and -gtest packages
echo "-- ensuring LLVM static libraries --"
apk update
LLVM_VER=$(ls -d /usr/lib/llvm* 2>/dev/null | sed 's|.*/llvm||' | sort -n | tail -1)
if [ -n "$LLVM_VER" ]; then
    apk add --no-cache llvm${LLVM_VER}-static llvm${LLVM_VER}-gtest
fi

# ensure CMAKE_PREFIX_PATH includes LLVM (for images without it in env.sh)
if [ -z "${CMAKE_PREFIX_PATH}" ]; then
    LLVM_PREFIX=$(ls -d /usr/lib/llvm* 2>/dev/null | sort -V | tail -1)
    if [ -n "$LLVM_PREFIX" ]; then
        export CMAKE_PREFIX_PATH="${LLVM_PREFIX}"
    fi
fi

# install tree-sitter CLI for astparser module build
if ! command -v tree-sitter > /dev/null 2>&1; then
    echo && echo "-- installing tree-sitter CLI --"
    cargo install tree-sitter-cli@0.26.5
fi

# build Qore Debug and Release in parallel
echo && echo "-- building Qore Debug and Release in parallel --"
cd ${QORE_SRC_DIR}

# Build Debug in background
(
    echo "Building Debug mode..."
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=debug -DSINGLE_COMPILATION_UNIT=1 -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    make -j${MAKE_JOBS}
    make install
    echo "Debug build complete"
) &
DEBUG_BUILD_PID=$!

# Build Release in background
(
    echo "Building Release mode..."
    mkdir build-release
    cd build-release
    cmake .. -DCMAKE_BUILD_TYPE=release -DSINGLE_COMPILATION_UNIT=1 -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    make -j${MAKE_JOBS}
    echo "Release build complete"
) &
RELEASE_BUILD_PID=$!

# Wait for both builds to complete
echo "Waiting for builds to complete..."
if ! wait $DEBUG_BUILD_PID; then
    echo "Debug build failed"
    exit 1
fi
if ! wait $RELEASE_BUILD_PID; then
    echo "Release build failed"
    exit 1
fi
echo "Both builds completed successfully"

# add Qore user and group
if ! grep -q "^qore:x:${QORE_GID}" /etc/group; then
    addgroup -g ${QORE_GID} qore
fi
if ! grep -q "^qore:x:${QORE_UID}" /etc/passwd; then
    adduser -u ${QORE_UID} -D -G qore -h /home/qore -s /bin/bash qore
fi

# own everything by the qore user
chown -R qore:qore ${QORE_SRC_DIR}

# run the tests
export QORE_MODULE_DIR=${QORE_SRC_DIR}/qlib:${QORE_MODULE_DIR}
cd ${QORE_SRC_DIR}

# Set up binary selector that uses Release for performance-sensitive tests
chmod +x ./test/docker_test/qore_binary_selector.sh
export QORE_DEBUG_BINARY="${QORE_SRC_DIR}/build/qore"
export QORE_RELEASE_BINARY="${QORE_SRC_DIR}/build-release/qore"
export QORE_BINARY="${QORE_SRC_DIR}/test/docker_test/qore_binary_selector.sh"

# Set up LIBQORE path for run_tests.sh (installed from Debug build)
if [ -f "${QORE_SRC_DIR}/build/libqore.so" ]; then
    export LIBQORE_BINARY="${QORE_SRC_DIR}/build/libqore.so"
elif [ -f "${QORE_SRC_DIR}/build/libqore.dylib" ]; then
    export LIBQORE_BINARY="${QORE_SRC_DIR}/build/libqore.dylib"
fi

# Run tests with binary selector (Debug for most tests, Release for WebSocketH2PerfTest)
# Pass environment variables explicitly to gosu to ensure they're available in the test process
echo && echo "-- running all tests (WebSocketH2PerfTest in Release mode, others in Debug) --"
gosu qore:qore env \
    QORE_DEBUG_BINARY="${QORE_DEBUG_BINARY}" \
    QORE_RELEASE_BINARY="${QORE_RELEASE_BINARY}" \
    QORE_BINARY="${QORE_BINARY}" \
    QORE_MODULE_DIR="${QORE_MODULE_DIR}" \
    LIBQORE_BINARY="${LIBQORE_BINARY}" \
    ./run_tests.sh

if [ "${drop_pgsql_schema}" = "1" ]; then
    cleanup_postgres_on_host
fi
