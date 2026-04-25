#!/bin/sh

set -e
set -x

# Performance test script - installs pre-built Release artifacts and runs only perf tests.
# No database services are required.

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

# install from pre-built artifact
#
# Use `cmake --install` directly instead of `make install`.  The build-perf
# job strips intermediate *.o files from the build/ artifact to keep its
# size down, but `make install` depends transitively on `make all` and
# therefore recompiles + relinks libqore + qore (~2 min wasted on the
# single-compilation-unit) just to satisfy missing intermediates that are
# not actually needed for installation.  `cmake --install` runs the install
# rule directly and only copies the final binaries / headers / .qm files
# the artifact already contains.
echo && echo "-- installing Qore (Release) --"
cmake --install ${QORE_SRC_DIR}/build

# run only performance tests
export QORE_MODULE_DIR=${QORE_SRC_DIR}/qlib:${QORE_MODULE_DIR}
cd ${QORE_SRC_DIR}
./run_tests.sh -P
