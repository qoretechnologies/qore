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

find / -path "${QORE_SRC_DIR}/build" -prune -o -name "libqore.so*" -exec rm -f {} \; 2>/dev/null || true

# build or install Qore
if [ -d "${QORE_SRC_DIR}/build" ] && [ -f "${QORE_SRC_DIR}/build/CMakeCache.txt" ]; then
    # Pre-built artifact from build stage - just install
    echo && echo "-- installing pre-built Qore --"
    cd ${QORE_SRC_DIR}/build
    cmake --install .
else
    # No pre-built artifact - full build
    # install tree-sitter CLI for astparser module build
    if ! command -v tree-sitter > /dev/null 2>&1; then
        echo && echo "-- installing tree-sitter CLI --"
        cargo install tree-sitter-cli@0.26.5
    fi

    export MAKE_JOBS=${MAKE_JOBS:-6}

    echo && echo "-- building Qore --"
    cd ${QORE_SRC_DIR}
    mkdir -p build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-debug} -DSINGLE_COMPILATION_UNIT=1 -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}
    make -j${MAKE_JOBS}
    make install
fi

# add Qore user and group
if ! grep -q "^qore:x:${QORE_GID}" /etc/group; then
    addgroup -g ${QORE_GID} qore
fi
if ! grep -q "^qore:x:${QORE_UID}" /etc/passwd; then
    adduser -u ${QORE_UID} -D -G qore -h /home/qore -s /bin/bash qore
fi

# Install gdb for crash diagnostics (captures backtraces on segfault).  This
# is strictly optional — gdb is only used to post-process core dumps after a
# segfault, and the tests themselves do not depend on it.  A failed install
# must not abort the test run.
#
# Alpine has broken apk installs in the past when the pinned base-image musl
# version (e.g. 1.2.5-r21) skews behind the repo's current musl-dbg
# dependency pin (e.g. 1.2.5-r22 with [!musl<1.2.5-r22]), causing apk to
# refuse the install with a long "unable to select packages" dependency
# chain.  When that happens, attempt an `apk upgrade` first to bring musl
# into sync; if that still fails, carry on without gdb — tests will just
# lose backtrace capture on segfault, which is diagnostic, not functional.
if ! command -v gdb > /dev/null 2>&1; then
    if ! apk add --no-cache gdb 2>/dev/null; then
        echo "WARNING: initial gdb install failed (likely apk repo drift vs. base image);"
        echo "WARNING: attempting 'apk upgrade' + retry..."
        apk upgrade --no-cache 2>/dev/null || true
        if ! apk add --no-cache gdb 2>/dev/null; then
            echo "WARNING: gdb install still failing — continuing without crash backtrace support"
        fi
    fi
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
gosu qore:qore ./run_tests.sh

if [ "${drop_pgsql_schema}" = "1" ]; then
    cleanup_postgres_on_host
fi
