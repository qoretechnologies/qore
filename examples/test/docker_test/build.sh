#!/bin/sh

set -e
set -x

# authenticate the clones CMake makes from github.com (see github-auth.sh)
. "$(dirname "$0")/github-auth.sh"

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
export QORE_SRC_DIR

export MAKE_JOBS=${MAKE_JOBS:-6}

${QORE_SRC_DIR}/test/docker_test/print-ci-provenance.sh || true

# the development files that the image may lack (see ci-build-deps.sh)
. "$(dirname "$0")/ci-build-deps.sh"
qore_ci_install_build_deps

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
# the qmod build cache, when the job has one (see qmod-cache.sh)
. "${QORE_SRC_DIR}/test/docker_test/qmod-cache.sh"
qore_qmod_cache_setup "${QORE_SRC_DIR}"
cmake .. -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-debug} -DQORE_QCC_TEST_HOOKS=ON -DSINGLE_COMPILATION_UNIT=1 -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} \
    ${QMOD_CACHE_ARG:+"${QMOD_CACHE_ARG}"}
make -j${MAKE_JOBS}
qore_qmod_cache_finish "${QORE_SRC_DIR}" "${QORE_SRC_DIR}/build"

# Remove intermediate object files to reduce artifact size
find . -name "*.o" -delete 2>/dev/null || true

{
    echo "qore_ci_build_artifact_info_version=1"
    printf "created_utc=%s\n" "$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    printf "ci_job_name=%s\n" "${CI_JOB_NAME:-unknown}"
    printf "ci_job_image=%s\n" "${CI_JOB_IMAGE:-unknown}"
    printf "ci_commit_sha=%s\n" "${CI_COMMIT_SHA:-unknown}"
    printf "qore_source_head=%s\n" "$(git -C "${QORE_SRC_DIR}" rev-parse HEAD 2>/dev/null || echo unknown)"
    printf "cmake_build_type=%s\n" "${CMAKE_BUILD_TYPE:-debug}"
    if [ -f /etc/qore-test-base-build-info ]; then
        echo
        echo "[qore-test-base-build-info]"
        cat /etc/qore-test-base-build-info
    fi
    if [ -x "${QORE_SRC_DIR}/build/qore" ]; then
        echo
        echo "[built-qore-version]"
        # the built qore with the built libqore, as the tests run it: the image's LD_LIBRARY_PATH names the library
        # directory of the image's Qore installation, which is searched before the build's run path and holds an older
        # libqore
        LD_LIBRARY_PATH="${QORE_SRC_DIR}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
            "${QORE_SRC_DIR}/build/qore" -V 2>&1 || true
    fi
} > "${QORE_SRC_DIR}/build/qore-ci-build-artifact-info"
