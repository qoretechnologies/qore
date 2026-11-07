# The qmod build cache in CI (design/qmod-build-cache.md); sourced by build.sh, build-perf.sh and test-macos.sh.
#
# With QORE_QMOD_CACHE_DIR set (the GitLab cache directory of the job), qore_qmod_cache_cmake_args prints the CMake
# argument that enables the cache, and qore_qmod_cache_finish, run after the build:
# - runs the guard, which fails when a qmod the cache supplied is not what it verified, or when any qmod of the build
#   does not carry the build's AOT runtime identity;
# - prints the hits and misses of the build and the size of the cache;
# - prunes the cache to the entries of this build, so it holds one build's modules.
# Without QORE_QMOD_CACHE_DIR both do nothing.
#
# Copyright (C) 2026 Qore Technologies, s.r.o.

qore_qmod_cache_cmake_args() {
    if [ -n "${QORE_QMOD_CACHE_DIR:-}" ]; then
        mkdir -p "${QORE_QMOD_CACHE_DIR}"
        echo "-DQORE_QMOD_CACHE_DIR=${QORE_QMOD_CACHE_DIR}"
    fi
}

# qore_qmod_cache_finish <source dir> <build dir>
qore_qmod_cache_finish() {
    [ -n "${QORE_QMOD_CACHE_DIR:-}" ] || return 0
    _qmc_tool="$1/tools/qore-qmod-cache.py"
    _qmc_build="$2"
    if ! env LD_LIBRARY_PATH="${_qmc_build}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
            python3 -I "${_qmc_tool}" verify --build-dir "${_qmc_build}" --qcc "${_qmc_build}/qcc" --require-sidecars; then
        echo "the qmod build cache guard failed: a module of this build is not what the cache verified (see above)"
        return 1
    fi
    python3 -I "${_qmc_tool}" stats --cache-dir "${QORE_QMOD_CACHE_DIR}" --build-dir "${_qmc_build}" || return 1
    python3 -I "${_qmc_tool}" prune --cache-dir "${QORE_QMOD_CACHE_DIR}" --keep-build "${_qmc_build}" || return 1
}
