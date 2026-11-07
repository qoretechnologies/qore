# The qmod build cache in CI (design/qmod-build-cache.md); sourced by build.sh, build-perf.sh and test-macos.sh.
#
# QORE_QMOD_CACHE_DIR is a host directory that the runner mounts into every job (the runner sets the variable in the
# job's environment; see design/qmod-build-cache.md "CI").  It has a directory per trust level:
# - protected/<job name>: written only by the jobs of protected refs (CI_COMMIT_REF_PROTECTED=true), which sign the
#   entries they store with QORE_QMOD_CACHE_SIGNING_KEY (a protected CI variable) and use only signed entries; a
#   runner that runs jobs of other refs mounts protected/ read-only;
# - unprotected/<ref>/<job name>: written by the jobs of one unprotected ref, which also read the protected cache of
#   their job name, but never write to it.
# Without the variable, or when the directory is not mounted, the build runs without the cache.
#
# - qore_qmod_cache_setup sets QMOD_CACHE_ARG and QMOD_CACHE_READ_ARG, the CMake arguments that enable the cache (empty
#   without one), and exports QORE_QMOD_CACHE_READONLY=1 when the job can only read its cache; it must run in the
#   shell of the build (not in a subshell);
# - qore_qmod_cache_finish, run after the build:
#   - runs the guard, which fails when a qmod the cache supplied is not what it verified, or when any qmod of the build
#     does not carry the build's AOT runtime identity;
#   - prunes the cache the job writes to: entries unused for QORE_QMOD_CACHE_MAX_AGE_DAYS days (default 7), then the
#     least recently used ones while it is larger than QORE_QMOD_CACHE_MAX_SIZE_MB MiB (default 3072); an entry that a
#     concurrent job is reading is never removed;
#   - prints one line with the hits and misses of the build and the size of the cache before and after pruning.
#
# Copyright (C) 2026 Qore Technologies, s.r.o.

# prints a name usable as a directory name
qore_qmod_cache_name() {
    printf '%s' "$1" | tr -c 'A-Za-z0-9._-' '_'
}

# prints a path without spaces for a directory (CMake gets the path through a link in the project directory when the
# real path has spaces, as the shared directories of a macOS VM under "/Volumes/My Shared Files" have)
qore_qmod_cache_link() {
    case "$1" in
        *" "*)
            _qmc_link="${CI_PROJECT_DIR:-$(pwd)}/$2"
            rm -f "${_qmc_link}" && ln -s "$1" "${_qmc_link}" && echo "${_qmc_link}"
            ;;
        *)
            echo "$1"
            ;;
    esac
}

qore_qmod_cache_setup() {
    QMOD_CACHE_ARG=
    QMOD_CACHE_READ_ARG=
    QMOD_CACHE_PRUNE_DIR=
    QMOD_CACHE_OWN_DIR=
    if [ -z "${QORE_QMOD_CACHE_DIR:-}" ]; then
        echo "qmod-cache: QORE_QMOD_CACHE_DIR is not set: building without the cache"
        return 0
    fi
    if [ ! -d "${QORE_QMOD_CACHE_DIR}" ]; then
        echo "qmod-cache: '${QORE_QMOD_CACHE_DIR}' is not mounted: building without the cache"
        return 0
    fi
    _qmc_job=$(qore_qmod_cache_name "${CI_JOB_NAME:-local}")
    _qmc_prot="${QORE_QMOD_CACHE_DIR}/protected"
    _qmc_own=
    _qmc_read=
    if [ "${CI_COMMIT_REF_PROTECTED:-false}" = "true" ]; then
        if [ -d "${_qmc_prot}" ] && [ -w "${_qmc_prot}" ]; then
            _qmc_own="${_qmc_prot}/${_qmc_job}"
            mkdir -p "${_qmc_own}" || _qmc_own=
            QMOD_CACHE_PRUNE_DIR="${_qmc_own}"
        elif [ -d "${_qmc_prot}/${_qmc_job}" ]; then
            # a runner that mounts the protected cache read-only
            _qmc_own="${_qmc_prot}/${_qmc_job}"
            export QORE_QMOD_CACHE_READONLY=1
        fi
    else
        _qmc_unprot="${QORE_QMOD_CACHE_DIR}/unprotected"
        if [ -d "${_qmc_unprot}" ] && [ -w "${_qmc_unprot}" ]; then
            _qmc_own="${_qmc_unprot}/$(qore_qmod_cache_name "${CI_COMMIT_REF_SLUG:-local}")/${_qmc_job}"
            mkdir -p "${_qmc_own}" || _qmc_own=
            QMOD_CACHE_PRUNE_DIR="${_qmc_own}"
        fi
        if [ -d "${_qmc_prot}/${_qmc_job}" ]; then
            _qmc_read="${_qmc_prot}/${_qmc_job}"
        fi
        if [ -z "${_qmc_own}" ] && [ -n "${_qmc_read}" ]; then
            _qmc_own="${_qmc_read}"
            _qmc_read=
            export QORE_QMOD_CACHE_READONLY=1
        fi
        # the signing key belongs to the protected level only
        unset QORE_QMOD_CACHE_SIGNING_KEY
    fi
    if [ -z "${_qmc_own}" ]; then
        echo "qmod-cache: no cache of this job's trust level is mounted in '${QORE_QMOD_CACHE_DIR}': building without the cache"
        return 0
    fi
    QMOD_CACHE_OWN_DIR=$(qore_qmod_cache_link "${_qmc_own}" .qmod-cache)
    QMOD_CACHE_ARG="-DQORE_QMOD_CACHE_DIR=${QMOD_CACHE_OWN_DIR}"
    if [ -n "${_qmc_read}" ]; then
        QMOD_CACHE_READ_ARG="-DQORE_QMOD_CACHE_READ_DIRS=$(qore_qmod_cache_link "${_qmc_read}" .qmod-cache-read)"
    fi
    if [ -n "${QMOD_CACHE_PRUNE_DIR}" ]; then
        QMOD_CACHE_PRUNE_DIR="${QMOD_CACHE_OWN_DIR}"
    fi
    echo "qmod-cache: using ${_qmc_own}${QORE_QMOD_CACHE_READONLY:+ (read-only)}${_qmc_read:+, reading ${_qmc_read}}"
}

# qore_qmod_cache_finish <source dir> <build dir>
qore_qmod_cache_finish() {
    [ -n "${QMOD_CACHE_OWN_DIR:-}" ] || return 0
    _qmc_tool="$1/tools/qore-qmod-cache.py"
    _qmc_build="$2"
    if ! env LD_LIBRARY_PATH="${_qmc_build}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
            python3 -I "${_qmc_tool}" verify --build-dir "${_qmc_build}" --qcc "${_qmc_build}/qcc" --require-sidecars; then
        echo "the qmod build cache guard failed: a module of this build is not what the cache verified (see above)"
        return 1
    fi
    if [ -n "${QMOD_CACHE_PRUNE_DIR}" ]; then
        python3 -I "${_qmc_tool}" prune --cache-dir "${QMOD_CACHE_PRUNE_DIR}" --build-dir "${_qmc_build}" \
            --job "${CI_JOB_NAME:-local}" --max-age-days "${QORE_QMOD_CACHE_MAX_AGE_DAYS:-7}" \
            --max-size "${QORE_QMOD_CACHE_MAX_SIZE_MB:-3072}" || return 1
    else
        python3 -I "${_qmc_tool}" stats --cache-dir "${QMOD_CACHE_OWN_DIR}" --build-dir "${_qmc_build}" || return 1
    fi
}
