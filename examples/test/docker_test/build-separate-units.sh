#!/bin/sh

# Builds the core library, qore and qcc, and the binary modules that have a single compilation unit build, from
# separate compilation units (-DSINGLE_COMPILATION_UNIT=OFF), without the AOT modules and without tests.  The other CI
# builds compile each of them as one unit, in which a source file that uses a declaration it does not include, or a
# template that it does not instantiate, still compiles and links: this build finds such a file (batch I needed three
# fixes of this kind, found by builds outside CI).  It stops at the first error.

set -e
set -x

# authenticate the clones CMake makes from github.com (see github-auth.sh)
. "$(dirname "$0")/github-auth.sh"

# Source environment from the Docker image (provides INSTALL_PREFIX, etc.)
ENV_FILE=/tmp/env.sh
if [ -f "${ENV_FILE}" ]; then
    . ${ENV_FILE}
fi

# setup QORE_SRC_DIR env var
cwd=`pwd`
if [ "${QORE_SRC_DIR}" = "" ]; then
    if [ -e "$cwd/cmake/QoreMacros.cmake" ] || [ -e "$cwd/lib/QoreLib.cpp" ]; then
        QORE_SRC_DIR=$cwd
    else
        QORE_SRC_DIR=$WORKDIR/qore
    fi
fi
export QORE_SRC_DIR

export MAKE_JOBS=${MAKE_JOBS:-6}

${QORE_SRC_DIR}/test/docker_test/print-ci-provenance.sh || true

# the development files that the image may lack (see ci-build-deps.sh)
. "$(dirname "$0")/ci-build-deps.sh"
qore_ci_install_build_deps

echo && echo "-- building Qore from separate compilation units --"
BUILD_DIR=${QORE_SEPARATE_UNITS_BUILD_DIR:-${QORE_SRC_DIR}/build-separate-units}
cmake -S "${QORE_SRC_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-debug} \
    -DSINGLE_COMPILATION_UNIT=OFF -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX:-/usr}
# the targets whose sources the option changes: libqore (linked by qore and qcc) and the binary modules with a single
# compilation unit build (the SINGLE_COMPILATION_UNIT cases in modules/*/CMakeLists.txt)
cmake --build "${BUILD_DIR}" -j${MAKE_JOBS} --target libqore qore qcc reflection krb5 logger_bin astparser
