#!/bin/sh

# The ThreadSanitizer job (test-tsan-ubuntu-amd64): builds qore, qcc and the binary modules that its tests load as a
# Debug build with -fsanitize=thread in the C, C++ and linker flags (a single compilation unit build, as the other CI
# builds), then runs with it the tests that compile C++ fixtures against libqore (which must get its sanitizer flags,
# qore#6587 and qore#6682) and the qore/threads and qore/debug suites, with the options of "Running Tests Under
# ThreadSanitizer" in CLAUDE.md.  The job fails when a test fails or when TSan writes a report (a report also makes
# the process exit with 66, but a report of a child process that a test does not check would be missed); the reports
# are printed in the log and kept with the job's artifacts (tsan-reports/).
#
# The job builds on its own, without the build job's artifact, so it starts with the pipeline; the times of the build
# and of the tests are printed at the end.

set -e
set -x

# authenticate the clones CMake makes from github.com (see github-auth.sh)
. "$(dirname "$0")/github-auth.sh"
# no process the job starts may outlive it (see ci-job-processes.sh)
. "$(dirname "$0")/ci-job-processes.sh"
qore_ci_mark_job_processes

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

# the LLVM of qcc (for images without it in env.sh)
if [ -z "${CMAKE_PREFIX_PATH}" ]; then
    LLVM_PREFIX=$(ls -d /usr/lib/llvm-* 2>/dev/null | sort -V | tail -1)
    if [ -n "$LLVM_PREFIX" ]; then
        export CMAKE_PREFIX_PATH="${LLVM_PREFIX}"
    fi
fi

cd ${QORE_SRC_DIR}
BUILD_DIR=${QORE_TSAN_BUILD_DIR:-${QORE_SRC_DIR}/build-tsan}
TSAN_FLAGS="-fsanitize=thread"

# the tests that compile a C++ fixture (a host executable, a module or a program that loads libqore) against the
# libqore of the build, with its sanitizer flags (qore#6587: the thread stack tests, qore#6682: the others); the tests
# of qore/threads and qore/debug are added below, as are the race tests of other directories (qore#6792) and the fork
# tests (qore#6817: their child processes run with the locks that the fork handlers release; fork-before-init runs
# qore-fork-before-init-test, a host executable of the build)
FIXTURE_TESTS="
examples/test/qore/threads/frame-check-stack.qtest
examples/test/qore/threads/parse-stack-boundary.qtest
examples/test/qore/threads/small-thread-stack.qtest
examples/test/ir/AOTConstructorParameterReceiverLifetime.qtest
examples/test/ir/AOTDeferredClassImpliedConstructor.qtest
examples/test/ir/AOTDeferredHashDeclImpliedConstructor.qtest
examples/test/ir/AOTQoLinkNativeSymbols.qtest
examples/test/ir/AOTRetainedMetadata.qtest
examples/test/ir/AOTScriptAggregateNativeRegister.qtest
examples/test/ir/AOTStaticModuleMetadata.qtest
examples/test/ir/LocalVarLValueLookup.qtest
examples/test/ir/OptionalMapTypeFolding.qtest
examples/test/ir/RecursiveClosureLocals.qtest
examples/test/qore/classes/Datasource/fork.qtest
examples/test/qore/classes/Program/parse-options-fork.qtest
examples/test/qore/classes/Socket/ssl-config-init.qtest
examples/test/qore/functions/fork_environment_lock_order.qtest
examples/test/qore/misc/dgc-removed-object-set-race.qtest
examples/test/qore/misc/fork-before-init/fork-before-init.qtest
examples/test/qore/misc/fork-delete-program-in-method.qtest
examples/test/qore/misc/fork-leaf-locks.qtest
examples/test/qore/misc/fork-thread-wait-lock.qtest
examples/test/qore/misc/libqore-dlopen/libqore-dlopen.qtest
examples/test/qore/misc/module-loader/aot-binary-symbol-resolution.qtest
examples/test/qore/misc/reflection-legacy-type-vec/reflection-legacy-type-vec.qtest
examples/test/qore/parser/modern-option-consistency/modern-option-consistency.qtest
examples/test/qore/parser/parse-stack-boundary/ParseStackBoundary-1.qtest
examples/test/qore/parser/parse-stack-boundary/ParseStackBoundary-2.qtest
examples/test/qore/parser/parse-stack-boundary/ParseStackBoundary-3.qtest
examples/test/qore/parser/parse-stack-boundary/ParseStackBoundary-4.qtest
examples/test/qore/parser/parse-stack-boundary/ParseStackBoundary-5.qtest
"
TSAN_TESTS=$( (for t in ${FIXTURE_TESTS}; do echo "$t"; done;
    find examples/test/qore/threads examples/test/qore/debug -name '*.qtest') | sort -u)
for t in ${TSAN_TESTS}; do
    if [ ! -f "$t" ]; then
        echo "ERROR: the TSan test $t does not exist" >&2
        exit 1
    fi
done

# TSan's memory layout is incompatible with the ASLR entropy of newer kernels (vm.mmap_rnd_bits above 28): a TSan
# process then re-executes itself with ASLR disabled, which fails in a container whose seccomp profile does not allow
# personality(ADDR_NO_RANDOMIZE) ("FATAL: ThreadSanitizer: encountered an incompatible memory layout but was unable to
# disable ASLR").  The build (which runs the TSan qpp) and the tests run with ASLR disabled when the container allows
# it, as every process inherits it: then no TSan process has to re-execute itself
NO_ASLR=""
if setarch "$(uname -m)" -R true 2>/dev/null; then
    NO_ASLR="setarch $(uname -m) -R"
else
    echo "-- ASLR cannot be disabled here (vm.mmap_rnd_bits=$(cat /proc/sys/vm/mmap_rnd_bits 2>/dev/null)) --"
fi

BUILD_START=$(date +%s)
echo "=== TSan build start: $(date -u '+%Y-%m-%dT%H:%M:%SZ') ==="
cmake -S "${QORE_SRC_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=debug -DQORE_QCC_TEST_HOOKS=ON \
    -DSINGLE_COMPILATION_UNIT=1 -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX:-/usr} \
    -DCMAKE_C_FLAGS="${TSAN_FLAGS}" -DCMAKE_CXX_FLAGS="${TSAN_FLAGS}" \
    -DCMAKE_EXE_LINKER_FLAGS="${TSAN_FLAGS}" -DCMAKE_SHARED_LINKER_FLAGS="${TSAN_FLAGS}" \
    -DCMAKE_MODULE_LINKER_FLAGS="${TSAN_FLAGS}"
# qcc, qore-aot-reseal and qore-fork-before-init-test: the tests that compile a program use the qcc next to qore (an
# installed one crashes with a TSan libqore), modern-option-consistency the qore-aot-reseal next to it and
# fork-before-init the qore-fork-before-init-test next to it (it skips without it); the binary modules that the tests
# load (json, reflection and yaml for QUnit and the test modules; logger_bin, i18n and linenoise for the debugger and
# sandbox tests of qore/debug and qore/threads; dbitest, the mock DBI driver of Datasource/fork, which run_tests.sh
# finds in the build's modules/ directory)
${NO_ASLR} cmake --build "${BUILD_DIR}" -j${MAKE_JOBS} --target qore qcc qore-aot-reseal qore-fork-before-init-test \
    json reflection yaml logger_bin i18n linenoise dbitest
BUILD_END=$(date +%s)
echo "=== TSan build end: $(date -u '+%Y-%m-%dT%H:%M:%SZ') ($((BUILD_END - BUILD_START)) s) ==="

# add Qore user and group: the tests run as the qore user, as in the other test jobs
QORE_UID=${QORE_UID:-999}
QORE_GID=${QORE_GID:-999}
if ! getent group qore >/dev/null 2>&1; then
    groupadd -o -g ${QORE_GID} qore
fi
if ! id qore >/dev/null 2>&1; then
    useradd -o -m -d /home/qore -u ${QORE_UID} -g ${QORE_GID} qore
fi

# cores (see ci-core-capture.sh) and TSan's reports are kept with the job's artifacts
. "${QORE_SRC_DIR}/test/docker_test/ci-core-capture.sh"
qore_ci_setup_core_capture
CORE_DIR="${QORE_SRC_DIR}/crash-dumps"
TSAN_REPORT_DIR="${QORE_SRC_DIR}/tsan-reports"
mkdir -p "${CORE_DIR}" "${TSAN_REPORT_DIR}"
chown -R qore:qore ${QORE_SRC_DIR}

TEST_LD_LIBRARY_PATH="${BUILD_DIR}"
# every run of the TSan build's qore uses the TSan build's libqore: this job does not install its build (unlike
# test-ubuntu.sh and test-alpine.sh), and the image's library path (/opt/lib, which LD_LIBRARY_PATH puts before the
# binary's own run path) has the image's older libqore, which lacks the symbols that a newer qore needs (a job failed
# with "undefined symbol: _Z31qore_set_module_cleanup_timeoutx" in the --module-dir call below)
export LD_LIBRARY_PATH="${TEST_LD_LIBRARY_PATH}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

# the native modules of the image that this qore loads, in a link farm of the run's own (see ci-native-modules.sh and
# test-ubuntu.sh)
. "${QORE_SRC_DIR}/test/docker_test/ci-native-modules.sh"
SYSTEM_BINARY_MODULE_DIR=$(${BUILD_DIR}/qore --module-dir)
CI_NATIVE_MODULE_DIR=$(mktemp -d "${TMPDIR:-/tmp}/qore-ci-native-modules.XXXXXX")
chmod 755 "${CI_NATIVE_MODULE_DIR}"
qore_ci_native_module_farm "${BUILD_DIR}/qore" "${CI_NATIVE_MODULE_DIR}" "${SYSTEM_BINARY_MODULE_DIR}"
export QORE_MODULE_DIR=${QORE_SRC_DIR}/qlib:${CI_NATIVE_MODULE_DIR}
export QORE_MODULE_DIR_ONLY=1

# see "Running Tests Under ThreadSanitizer" in CLAUDE.md
TSAN_OPTIONS="suppressions=${QORE_SRC_DIR}/test/cpp/tsan.supp die_after_fork=0 report_thread_leaks=0 log_path=${TSAN_REPORT_DIR}/tsan"

. "${QORE_SRC_DIR}/test/docker_test/ci-resource-report.sh"
qore_ci_resource_report "before the tests"
QORE_TEST_JOBS="${QORE_TEST_JOBS:-$(qore_ci_test_jobs)}"
echo "QORE_TEST_JOBS=${QORE_TEST_JOBS}"
echo "-- running $(echo ${TSAN_TESTS} | wc -w) tests with the TSan build --"
TEST_START=$(date +%s)
echo "=== TSan tests start: $(date -u '+%Y-%m-%dT%H:%M:%SZ') ==="
TEST_RC=0
${NO_ASLR} gosu qore:qore env \
    QORE_BINARY="${BUILD_DIR}/qore" \
    QORE_DEBUG_BINARY="${BUILD_DIR}/qore" \
    LIBQORE_BINARY="${BUILD_DIR}/libqore.so" \
    QORE_LIBDIR="${BUILD_DIR}" \
    QORE_MODULE_DIR="${QORE_MODULE_DIR}" \
    QORE_MODULE_DIR_ONLY="${QORE_MODULE_DIR_ONLY}" \
    QORE_TEST_JOBS="${QORE_TEST_JOBS}" \
    TSAN_OPTIONS="${TSAN_OPTIONS}" \
    LD_LIBRARY_PATH="${TEST_LD_LIBRARY_PATH}:${LD_LIBRARY_PATH}" \
    ./run_tests.sh ${TSAN_TESTS} || TEST_RC=$?
TEST_END=$(date +%s)
echo "=== TSan tests end: $(date -u '+%Y-%m-%dT%H:%M:%SZ') ($((TEST_END - TEST_START)) s) ==="
qore_ci_resource_report "after the tests"
qore_ci_collect_cores "$CORE_DIR"

set +x
REPORTS=$(find "${TSAN_REPORT_DIR}" -type f | sort)
for report in ${REPORTS}; do
    echo "=== TSan report: ${report} ==="
    cat "${report}"
done
echo "TSan build: $((BUILD_END - BUILD_START)) s, tests: $((TEST_END - TEST_START)) s, test result: ${TEST_RC}," \
    "TSan reports: $(echo ${REPORTS} | wc -w)"
set -x

if ! qore_ci_end_job_processes; then
    exit 1
fi
if [ "${TEST_RC}" != "0" ]; then
    exit ${TEST_RC}
fi
if [ -n "${REPORTS}" ]; then
    echo "ERROR: ThreadSanitizer reported errors (above)" >&2
    exit 1
fi
