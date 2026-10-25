#!/bin/sh

set -e
set -x

# no process the job starts may outlive it (see ci-job-processes.sh)
. "$(dirname "$0")/ci-job-processes.sh"
qore_ci_mark_job_processes

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
export QORE_SRC_DIR

export MAKE_JOBS=${MAKE_JOBS:-6}

${QORE_SRC_DIR}/test/docker_test/print-ci-provenance.sh || true

# The build artifact was compiled against the binary modules of the build job's qore-test-base image: a job on another
# image (the image tag was published again during the pipeline) fails before any test runs (see ci-build-image-check.sh)
"${QORE_SRC_DIR}/test/docker_test/ci-build-image-check.sh" \
    "${QORE_SRC_DIR}/build/qore-ci-build-artifact-info" || exit 1

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

# core dumps: the pattern is the one every CI job on the node sets, as the node has only one (see
# ci-core-capture.sh); cores are kept in crash-dumps/ (the job artifact)
. "${QORE_SRC_DIR}/test/docker_test/ci-core-capture.sh"
qore_ci_setup_core_capture
mkdir -p "${QORE_SRC_DIR}/crash-dumps"

# run only performance tests
export QORE_MODULE_DIR=${QORE_SRC_DIR}/qlib:${QORE_MODULE_DIR}
cd ${QORE_SRC_DIR}
TEST_RC=0
./run_tests.sh -P || TEST_RC=$?

# the cores of the processes the tests started are kept with those of the tests
qore_ci_collect_cores "${QORE_SRC_DIR}/crash-dumps"
# the runner ends the job only when no process the job started still holds its output
if ! qore_ci_end_job_processes && [ "${TEST_RC}" = "0" ]; then
    TEST_RC=1
fi
exit ${TEST_RC}
