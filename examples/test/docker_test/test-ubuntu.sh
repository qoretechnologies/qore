#!/bin/sh

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

echo "export QORE_UID=999" >> ${ENV_FILE}
echo "export QORE_GID=999" >> ${ENV_FILE}

. ${ENV_FILE}

if [ -z "${QORE_DB_CONNSTR_PGSQL}" ]; then
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

export MAKE_JOBS=${MAKE_JOBS:-6}

# ensure LLVM dev libraries are installed (needed for JIT/AOT)
if ! dpkg -l llvm-dev >/dev/null 2>&1; then
    echo "-- installing llvm-dev --"
    apt-get -y update && apt-get -y install --no-install-recommends llvm-dev
fi

# ensure CMAKE_PREFIX_PATH includes LLVM (for images without it in env.sh)
if [ -z "${CMAKE_PREFIX_PATH}" ]; then
    LLVM_PREFIX=$(ls -d /usr/lib/llvm-* 2>/dev/null | sort -V | tail -1)
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
    cmake .. -DCMAKE_BUILD_TYPE=debug -DSINGLE_COMPILATION_UNIT=1 -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}
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
    cmake .. -DCMAKE_BUILD_TYPE=release -DSINGLE_COMPILATION_UNIT=1 -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}
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
groupadd -o -g ${QORE_GID} qore
useradd -o -m -d /home/qore -u ${QORE_UID} -g ${QORE_GID} qore

# install gdb for crash diagnostics (captures backtraces on segfault)
if ! command -v gdb > /dev/null 2>&1; then
    apt-get update -qq && apt-get install -y -qq gdb > /dev/null 2>&1
fi

# Enable core dumps before dropping to qore user (needs root for /proc/sys writes)
# suid_dumpable=2 writes cores to the core_pattern location securely (needed for gosu)
if [ -f /proc/sys/fs/suid_dumpable ]; then
    echo 2 > /proc/sys/fs/suid_dumpable 2>/dev/null || true
fi
# Set core_pattern to write cores to crash-dumps/ (must be done as root)
CORE_DIR="${QORE_SRC_DIR}/crash-dumps"
mkdir -p "$CORE_DIR"
chown qore:qore "$CORE_DIR"
if [ -w /proc/sys/kernel/core_pattern ]; then
    echo "$CORE_DIR/core.%e.%p" > /proc/sys/kernel/core_pattern
elif command -v sysctl > /dev/null 2>&1; then
    sysctl -w "kernel.core_pattern=$CORE_DIR/core.%e.%p" 2>/dev/null || true
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
# Set LD_LIBRARY_PATH to ensure binaries use the correct libqore.so from build directory
echo && echo "-- running all tests (WebSocketH2PerfTest in Release mode, others in Debug) --"
gosu qore:qore env \
    QORE_DEBUG_BINARY="${QORE_DEBUG_BINARY}" \
    QORE_RELEASE_BINARY="${QORE_RELEASE_BINARY}" \
    QORE_BINARY="${QORE_BINARY}" \
    QORE_MODULE_DIR="${QORE_MODULE_DIR}" \
    LIBQORE_BINARY="${LIBQORE_BINARY}" \
    LD_LIBRARY_PATH="${QORE_SRC_DIR}/build/lib:${QORE_SRC_DIR}/build-release/lib:${QORE_SRC_DIR}/build/lib/.libs:${QORE_SRC_DIR}/build-release/lib/.libs:${LD_LIBRARY_PATH}" \
    QORE_SELECTOR_DEBUG=1 \
    ./run_tests.sh

if [ "${drop_pgsql_schema}" = "1" ]; then
    cleanup_postgres_on_host
fi
