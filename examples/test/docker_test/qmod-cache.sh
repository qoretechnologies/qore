# The qmod build cache in CI (design/qmod-build-cache.md); sourced by build.sh, build-perf.sh and test-macos.sh.
#
# QORE_QMOD_CACHE_DIR is a host directory that the runner mounts into every job (the runner sets the variable in the
# job's environment; see design/qmod-build-cache.md "CI").  Its cache is protected/<job name>:
# - the jobs of protected refs (CI_COMMIT_REF_PROTECTED=true) write it: they sign every entry they store with the
#   Ed25519 private key QORE_QMOD_CACHE_SIGNING_KEY (a protected CI variable, which only they get) and use only entries
#   signed with it; without the key, a job of a protected ref builds without the cache;
# - the jobs of other refs only read it: the runner that runs them mounts protected/ read-only, and they use only
#   entries that the public key QORE_QMOD_CACHE_VERIFY_KEY (not secret) verifies, so an entry that anyone else wrote is
#   a miss; they write no cache that another job reads.  Without the public key they build without the cache.
# Without QORE_QMOD_CACHE_DIR, or when protected/<job name> is not there, the build runs without the cache.
#
# - qore_qmod_cache_setup sets QMOD_CACHE_ARG, the CMake argument that enables the cache (empty without one), and
#   exports QORE_QMOD_CACHE_READONLY=1 when the job only reads the cache; it must run in the shell of the build (not in
#   a subshell);
# - qore_qmod_cache_finish, run after the build:
#   - runs the guard, which fails when a qmod the cache supplied is not what it verified (or not signed), or when any
#     qmod of the build does not carry the build's AOT runtime identity;
#   - unsets QORE_QMOD_CACHE_SIGNING_KEY, so that nothing run after the build (the tests of test-macos.sh) has it;
#   - in a job that writes the cache: prunes it (entries unused for QORE_QMOD_CACHE_MAX_AGE_DAYS days, default 7, then
#     the least recently used ones while it is larger than QORE_QMOD_CACHE_MAX_SIZE_MB MiB, default 3072; an entry that
#     a concurrent job is reading is never removed), and the caches of the other job names by age only, so that the
#     cache of a job that no longer runs empties out;
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
    QMOD_CACHE_PRUNE_DIR=
    QMOD_CACHE_OWN_DIR=
    if [ -z "${QORE_QMOD_CACHE_DIR:-}" ]; then
        echo "qmod-cache: QORE_QMOD_CACHE_DIR is not set: building without the cache"
        return 0
    fi
    _qmc_prot="${QORE_QMOD_CACHE_DIR}/protected"
    if [ ! -d "${_qmc_prot}" ]; then
        echo "qmod-cache: '${_qmc_prot}' is not mounted: building without the cache"
        return 0
    fi
    _qmc_job=$(qore_qmod_cache_name "${CI_JOB_NAME:-local}")
    _qmc_own="${_qmc_prot}/${_qmc_job}"
    if [ "${CI_COMMIT_REF_PROTECTED:-false}" = "true" ]; then
        if [ -z "${QORE_QMOD_CACHE_SIGNING_KEY:-}" ]; then
            echo "qmod-cache: a protected ref without QORE_QMOD_CACHE_SIGNING_KEY: building without the cache"
            return 0
        fi
        if [ -w "${_qmc_prot}" ]; then
            mkdir -p "${_qmc_own}" || return 0
            QMOD_CACHE_PRUNE_DIR=1
        else
            # a runner that mounts the protected cache read-only
            export QORE_QMOD_CACHE_READONLY=1
        fi
    else
        # the private key is the protected refs' only; this job only reads, and only signed entries
        unset QORE_QMOD_CACHE_SIGNING_KEY
        if [ -z "${QORE_QMOD_CACHE_VERIFY_KEY:-}" ]; then
            echo "qmod-cache: no QORE_QMOD_CACHE_VERIFY_KEY to verify the protected cache: building without the cache"
            return 0
        fi
        export QORE_QMOD_CACHE_READONLY=1
    fi
    if [ ! -d "${_qmc_own}" ]; then
        echo "qmod-cache: '${_qmc_own}' does not exist yet: building without the cache"
        return 0
    fi
    QMOD_CACHE_OWN_DIR=$(qore_qmod_cache_link "${_qmc_own}" .qmod-cache)
    QMOD_CACHE_ARG="-DQORE_QMOD_CACHE_DIR=${QMOD_CACHE_OWN_DIR}"
    if [ -n "${QMOD_CACHE_PRUNE_DIR}" ]; then
        QMOD_CACHE_PRUNE_DIR="${QMOD_CACHE_OWN_DIR}"
    fi
    echo "qmod-cache: using ${_qmc_own}${QORE_QMOD_CACHE_READONLY:+ (read-only)}"
}

# qore_qmod_cache_prune_others <tool>: prunes the caches of the other job names by age, so that the cache of a job
# that no longer runs (a renamed or removed job) empties out
qore_qmod_cache_prune_others() {
    _qmc_job=$(qore_qmod_cache_name "${CI_JOB_NAME:-local}")
    for _qmc_other in "${QORE_QMOD_CACHE_DIR}"/protected/*; do
        [ -d "${_qmc_other}" ] || continue
        [ "$(basename "${_qmc_other}")" = "${_qmc_job}" ] && continue
        python3 -I "$1" prune --cache-dir "${_qmc_other}" --max-age-days "${QORE_QMOD_CACHE_MAX_AGE_DAYS:-7}" \
            > /dev/null || return 1
    done
}

# qore_qmod_cache_finish <source dir> <build dir>
qore_qmod_cache_finish() {
    if [ -z "${QMOD_CACHE_OWN_DIR:-}" ]; then
        unset QORE_QMOD_CACHE_SIGNING_KEY
        return 0
    fi
    _qmc_tool="$1/tools/qore-qmod-cache.py"
    _qmc_build="$2"
    _qmc_rc=0
    if ! env LD_LIBRARY_PATH="${_qmc_build}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
            python3 -I "${_qmc_tool}" verify --build-dir "${_qmc_build}" --qcc "${_qmc_build}/qcc" --require-sidecars; then
        echo "the qmod build cache guard failed: a module of this build is not what the cache verified (see above)"
        _qmc_rc=1
    fi
    # nothing run after the build gets the private key
    unset QORE_QMOD_CACHE_SIGNING_KEY
    [ ${_qmc_rc} -eq 0 ] || return ${_qmc_rc}
    if [ -n "${QMOD_CACHE_PRUNE_DIR}" ]; then
        python3 -I "${_qmc_tool}" prune --cache-dir "${QMOD_CACHE_PRUNE_DIR}" --build-dir "${_qmc_build}" \
            --job "${CI_JOB_NAME:-local}" --max-age-days "${QORE_QMOD_CACHE_MAX_AGE_DAYS:-7}" \
            --max-size "${QORE_QMOD_CACHE_MAX_SIZE_MB:-3072}" || return 1
        qore_qmod_cache_prune_others "${_qmc_tool}" || return 1
    else
        python3 -I "${_qmc_tool}" stats --cache-dir "${QMOD_CACHE_OWN_DIR}" --build-dir "${_qmc_build}" || return 1
    fi
}
