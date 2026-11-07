# The qmod build cache in CI (design/qmod-build-cache.md); sourced by build.sh, build-perf.sh and test-macos.sh.
#
# QORE_QMOD_CACHE_DIR is a host directory that the runner mounts into every job (the runner sets the variable in the
# job's environment; see design/qmod-build-cache.md "CI"); a job's cache is the directory of the job's name in it,
# which the jobs of that name share.  Without the variable, or when the directory is not there or cannot be written,
# the build runs without the cache.
#
# - qore_qmod_cache_cmake_args prints the CMake argument that enables the cache, if any;
# - qore_qmod_cache_finish, run after the build:
#   - runs the guard, which fails when a qmod the cache supplied is not what it verified, or when any qmod of the build
#     does not carry the build's AOT runtime identity;
#   - prunes the cache: the entries unused for QORE_QMOD_CACHE_MAX_AGE_DAYS days (default 7), then the least
#     recently used ones while the cache is larger than QORE_QMOD_CACHE_MAX_SIZE_MB MiB (default 3072); an entry that
#     a concurrent job is reading is never removed;
#   - prints one line with the hits and misses of the build and the size of the cache before and after pruning.
#
# Copyright (C) 2026 Qore Technologies, s.r.o.

# prints the cache directory of this job, or nothing when the build runs without the cache
qore_qmod_cache_job_dir() {
    [ -n "${QORE_QMOD_CACHE_DIR:-}" ] || return 0
    [ -d "${QORE_QMOD_CACHE_DIR}" ] && [ -w "${QORE_QMOD_CACHE_DIR}" ] || return 0
    _qmc_job=$(printf '%s' "${CI_JOB_NAME:-local}" | tr -c 'A-Za-z0-9._-' '_')
    _qmc_dir="${QORE_QMOD_CACHE_DIR}/${_qmc_job}"
    mkdir -p "${_qmc_dir}" || return 0
    case "${_qmc_dir}" in
        *" "*)
            # a path with spaces (the shared directories of a macOS VM are under "/Volumes/My Shared Files") is
            # given to the build through a link without them
            _qmc_link="${CI_PROJECT_DIR:-$(pwd)}/.qmod-cache"
            rm -f "${_qmc_link}" && ln -s "${_qmc_dir}" "${_qmc_link}" || return 0
            _qmc_dir="${_qmc_link}"
            ;;
    esac
    echo "${_qmc_dir}"
}

qore_qmod_cache_cmake_args() {
    _qmc_dir=$(qore_qmod_cache_job_dir)
    if [ -z "${_qmc_dir}" ]; then
        if [ -n "${QORE_QMOD_CACHE_DIR:-}" ]; then
            echo "qmod-cache: '${QORE_QMOD_CACHE_DIR}' is not mounted or cannot be written: building without the cache" >&2
        else
            echo "qmod-cache: QORE_QMOD_CACHE_DIR is not set: building without the cache" >&2
        fi
        return 0
    fi
    echo "-DQORE_QMOD_CACHE_DIR=${_qmc_dir}"
}

# qore_qmod_cache_finish <source dir> <build dir>
qore_qmod_cache_finish() {
    _qmc_dir=$(qore_qmod_cache_job_dir)
    [ -n "${_qmc_dir}" ] || return 0
    _qmc_tool="$1/tools/qore-qmod-cache.py"
    _qmc_build="$2"
    if ! env LD_LIBRARY_PATH="${_qmc_build}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
            python3 -I "${_qmc_tool}" verify --build-dir "${_qmc_build}" --qcc "${_qmc_build}/qcc" --require-sidecars; then
        echo "the qmod build cache guard failed: a module of this build is not what the cache verified (see above)"
        return 1
    fi
    python3 -I "${_qmc_tool}" prune --cache-dir "${_qmc_dir}" --build-dir "${_qmc_build}" \
        --job "${CI_JOB_NAME:-local}" --max-age-days "${QORE_QMOD_CACHE_MAX_AGE_DAYS:-7}" \
        --max-size "${QORE_QMOD_CACHE_MAX_SIZE_MB:-3072}" || return 1
}
