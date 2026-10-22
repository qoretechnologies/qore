#!/bin/sh
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Runs the build-system tests in examples/test/cmake (the CMake macros and scripts, qpp, qcc and the documentation
# toolchain, with real CMake, Doxygen and compilers) against the build of the CI build job (the build/ artifact).
# They run as a user without root privileges, as the other tests do: one of them checks a directory permission, which
# root bypasses.

set -e
set -x

# no process the job starts may outlive it (see ci-job-processes.sh)
. "$(dirname "$0")/ci-job-processes.sh"
qore_ci_mark_job_processes

# setup QORE_SRC_DIR env var
cwd=`pwd`
if [ "${QORE_SRC_DIR}" = "" ]; then
    if [ -e "$cwd/qlib/SqlUtil.qm" ] || [ -e "$cwd/bin/qdbg" ] || [ -e "$cwd/cmake/QoreMacros.cmake" ] || [ -e "$cwd/lib/QoreLib.cpp" ]; then
        QORE_SRC_DIR=$cwd
    else
        QORE_SRC_DIR=$WORKDIR/qore
    fi
fi

${QORE_SRC_DIR}/test/docker_test/print-ci-provenance.sh || true

BUILD_DIR=${QORE_SRC_DIR}/build
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    echo "no Qore build in ${BUILD_DIR}: the tests run against the build job's build/ artifact"
    exit 1
fi

# the user the tests run as
if ! id qore > /dev/null 2>&1; then
    if command -v useradd > /dev/null 2>&1; then
        useradd -m -U qore
    else
        adduser -D qore
    fi
fi

# the tests take the build's tools from these variables (a qpp in the PATH would be the image's)
cd ${QORE_SRC_DIR}
TEST_RC=0
gosu qore env \
    QORE_QPP_EXECUTABLE="${BUILD_DIR}/qpp" \
    QORE_QCC_EXECUTABLE="${BUILD_DIR}/qcc" \
    QORE_BINARY="${BUILD_DIR}/qore" \
    QORE_TEST_BUILD_DIR="${BUILD_DIR}" \
    python3 -B -W error -m unittest discover -s examples/test/cmake -v || TEST_RC=$?

# the runner ends the job only when no process the job started still holds its output
if ! qore_ci_end_job_processes; then
    exit 1
fi
exit ${TEST_RC}
