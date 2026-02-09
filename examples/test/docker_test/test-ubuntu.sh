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

export MAKE_JOBS=6

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

# build Qore and install
echo && echo "-- building Qore --"
cd ${QORE_SRC_DIR}
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=debug -DSINGLE_COMPILATION_UNIT=1 -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}
make -j${MAKE_JOBS}
make install

# add Qore user and group
groupadd -o -g ${QORE_GID} qore
useradd -o -m -d /home/qore -u ${QORE_UID} -g ${QORE_GID} qore

# own everything by the qore user
chown -R qore:qore ${QORE_SRC_DIR}

# run the tests
export QORE_MODULE_DIR=${QORE_SRC_DIR}/qlib:${QORE_MODULE_DIR}
cd ${QORE_SRC_DIR}
gosu qore:qore ./run_tests.sh

if [ "${drop_pgsql_schema}" = "1" ]; then
    cleanup_postgres_on_host
fi
