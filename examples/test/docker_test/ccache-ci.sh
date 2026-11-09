# -*- mode: sh -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# The C and C++ compiler cache (ccache) of the macOS CI job; sourced by test-macos.sh.
#
# The job builds in a new VM, from a clean build directory, every time; ccache keeps the objects of the compiles of
# earlier jobs in a directory of the host that the runner mounts into the VM, the mount of the qmod build cache
# (QORE_QMOD_CACHE_DIR, see qmod-cache.sh and design/qmod-build-cache-runners.md): <QORE_QMOD_CACHE_DIR>/ccache, beside
# its protected/ directory.  The cache is used only when that directory exists and the image has ccache; otherwise the
# job builds without it, as before.  CCACHE_DISABLE=1 (ccache's own variable) compiles everything.
#
# Trust: as the qmod cache, the cache is written only by the jobs of protected refs, on the runner that mounts the
# directory read-write; the runner of every other ref mounts it read-only, so the jobs of other refs only read it.  A
# job of an unprotected ref that finds the directory writable reads it only as well (CCACHE_READONLY).
#
# Correctness: ccache reuses an object only when the compiler, its command line and the preprocessed source (in
# direct mode: the source and the content of every file it includes) are the same, so an object is never reused for
# other sources, and a compile that fails is never cached, so the cache hides no build failure.  The compiler is
# checked by its "--version" output (Apple's clang reports its build), and the cache of each toolchain (the
# architecture, the macOS and SDK versions, the compiler and ccache) is a directory of its own, so an image with
# another toolchain starts an empty cache instead of reusing objects that another one compiled.  The build directory
# is still removed before every build, and the link, qpp, bison and flex steps run every time.
#
# - qore_ccache_setup sets CCACHE_CMAKE_ARGS, the CMake arguments that compile through ccache (empty without the
#   cache), and exports the CCACHE_* settings of the build; it must run in the shell of the build (not a subshell)
# - qore_ccache_finish prints the hits and misses of the build; in a job that writes the cache, it also removes the
#   caches of the toolchains that no job has used for QORE_CCACHE_MAX_AGE_DAYS days (default 30); it unsets the
#   CCACHE_* settings, so that nothing the tests compile goes through the cache
# The size of a toolchain's cache is QORE_CCACHE_MAX_SIZE (default 8G; ccache removes the least recently used objects
# above it).

# prints a key of the toolchain that compiles the build
qore_ccache_toolchain_key() {
    {
        uname -m
        sw_vers -productVersion 2>/dev/null
        xcrun --show-sdk-version 2>/dev/null
        cc --version 2>&1
        c++ --version 2>&1
        ccache --version 2>&1 | head -1
    } | shasum -a 256 | cut -c1-16
}

qore_ccache_setup() {
    CCACHE_CMAKE_ARGS=
    QORE_CCACHE_WRITER=
    QORE_CCACHE_KEY_DIR=
    if [ -z "${QORE_QMOD_CACHE_DIR:-}" ] || [ ! -d "${QORE_QMOD_CACHE_DIR}/ccache" ]; then
        echo "ccache: no cache directory (\$QORE_QMOD_CACHE_DIR/ccache) is mounted: building without ccache"
        return 0
    fi
    if ! command -v ccache > /dev/null 2>&1; then
        echo "ccache: the image has no ccache (Homebrew package ccache): building without ccache"
        return 0
    fi
    if [ "${CCACHE_DISABLE:-}" = "1" ]; then
        echo "ccache: CCACHE_DISABLE=1: building without ccache"
        return 0
    fi
    _qcc_base="${QORE_QMOD_CACHE_DIR}/ccache"
    QORE_CCACHE_KEY_DIR="${_qcc_base}/$(qore_ccache_toolchain_key)"
    if [ "${CI_COMMIT_REF_PROTECTED:-false}" = "true" ] && [ -w "${_qcc_base}" ]; then
        if ! mkdir -p "${QORE_CCACHE_KEY_DIR}"; then
            echo "ccache: cannot create ${QORE_CCACHE_KEY_DIR}: building without ccache"
            QORE_CCACHE_KEY_DIR=
            return 0
        fi
        QORE_CCACHE_WRITER=1
        # the last job that used the toolchain's cache (see qore_ccache_finish)
        touch "${QORE_CCACHE_KEY_DIR}/.last-used"
        CCACHE_MAXSIZE="${QORE_CCACHE_MAX_SIZE:-8G}"
        export CCACHE_MAXSIZE
    else
        if [ ! -d "${QORE_CCACHE_KEY_DIR}" ]; then
            echo "ccache: no cache of this toolchain in ${_qcc_base} yet: building without ccache"
            QORE_CCACHE_KEY_DIR=
            return 0
        fi
        # only the jobs of protected refs write the cache; nothing is written to the directory, not even statistics
        CCACHE_READONLY=1
        CCACHE_NOSTATS=1
        export CCACHE_READONLY CCACHE_NOSTATS
    fi
    CCACHE_DIR="${QORE_CCACHE_KEY_DIR}"
    CCACHE_COMPILERCHECK="%compiler% --version"
    # the temporary files of the compiles stay in the VM; the statistics of this build are logged there too
    CCACHE_TEMPDIR=$(mktemp -d "${TMPDIR:-/tmp}/qore-ccache-tmp.XXXXXX")
    CCACHE_STATSLOG="${CCACHE_TEMPDIR}/stats.log"
    export CCACHE_DIR CCACHE_COMPILERCHECK CCACHE_TEMPDIR CCACHE_STATSLOG
    CCACHE_CMAKE_ARGS="-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache"
    echo "ccache: using ${QORE_CCACHE_KEY_DIR}${CCACHE_READONLY:+ (read-only)}"
}

qore_ccache_finish() {
    if [ -z "${QORE_CCACHE_KEY_DIR}" ]; then
        return 0
    fi
    echo "ccache: the compiles of this build:"
    ccache --show-log-stats -v || true
    if [ -n "${QORE_CCACHE_WRITER}" ]; then
        ccache --show-stats || true
        # the caches of the toolchains that no job used for QORE_CCACHE_MAX_AGE_DAYS days, as of an older image
        for _qcc_dir in "${QORE_QMOD_CACHE_DIR}"/ccache/*; do
            [ -d "${_qcc_dir}" ] || continue
            [ "${_qcc_dir}" = "${QORE_CCACHE_KEY_DIR}" ] && continue
            if [ -n "$(find "${_qcc_dir}/.last-used" -mtime "+${QORE_CCACHE_MAX_AGE_DAYS:-30}" 2>/dev/null)" ]; then
                echo "ccache: removing ${_qcc_dir}, unused for ${QORE_CCACHE_MAX_AGE_DAYS:-30} days"
                rm -rf "${_qcc_dir}"
            fi
        done
    fi
    rm -rf "${CCACHE_TEMPDIR}"
    # the compiles that the tests make do not go through the cache
    unset CCACHE_DIR CCACHE_READONLY CCACHE_NOSTATS CCACHE_COMPILERCHECK CCACHE_TEMPDIR CCACHE_STATSLOG CCACHE_MAXSIZE
}
