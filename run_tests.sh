#!/bin/sh
# Copyright (C) 2026 Qore Technologies, s.r.o.

print_usage () {
  echo "Usage: run_tests.sh [OPTIONS] [TEST...]"
  echo "Run qore tests: the given test files, or else the tests in the test directories."
  echo
  echo "  -d <dir>   Run only specified tests (as found in $BASE_TEST_PATH)."
  echo "  -j         Use --format=junit option for the tests, making them print JUnit output."
  echo "  -l         List the tests that would run (after the selection and the shard), one per line, and exit."
  echo "  -E         Exclude performance/stress tests (matched by *Perf* or PipelineMemory pattern)."
  echo "  -P         Run only performance/stress tests, and the tests that check a release build"
  echo "             ('# test-release-build: <why>')."
  echo "  -t         Measure execution time of the tests."
  echo "  -v         Use --format=plain option for the tests, making them print one statement per each test case."
  echo
  echo "Environment variables:"
  echo "  QORE_TEST_OPTS           Additional options to pass to qore (e.g., '-penable-debug')."
  echo "  QORE_TEST_DIRS           Space-separated test directories (as found in $BASE_TEST_PATH) to run when no"
  echo "                           -d option is given (e.g., 'qore/misc qore/vars'); default: all tests."
  echo "  QORE_TEST_EXTRA_DIRS     Space-separated directories outside $BASE_TEST_PATH (relative to the current"
  echo "                           directory or absolute) whose tests are run as well (e.g., 'modules/ml/test');"
  echo "                           the tests of the binary modules in ./modules are run only when named here."
  echo "  TEST_TIMEOUT             Seconds a single test may run before it is killed (default: 300); a test"
  echo "                           declares its own budget with a '# test-timeout: <seconds>' comment.  A test"
  echo "                           that exceeds it is first sent SIGUSR1 if it handles it (QUnit tests print the"
  echo "                           call stacks of their threads), then aborted (SIGABRT), so that it dumps core;"
  echo "                           a test that does not handle SIGUSR1, or one run with signal handling disabled"
  echo "                           (-b in QORE_TEST_OPTS), is aborted at once.  On Linux, the state, CPU time and"
  echo "                           kernel wait channel of each of its threads are printed when it exceeds it and"
  echo "                           again before it is aborted, with /proc/pressure and the cgroup's cpu.stat and"
  echo "                           memory.events.  Where perl or setsid is available, the test runs in a process"
  echo "                           group of its own, and the processes left in it are killed once the test has"
  echo "                           ended, as they are when a signal ends the run; otherwise the test is signalled"
  echo "                           alone.  In a run from a terminal, the test is then a background process group:"
  echo "                           with 'stty tostop', or if it changes the terminal's attributes, it is stopped"
  echo "                           by SIGTTOU."
  echo "  QORE_TEST_CGROUP         The cgroup directory whose cpu.stat and memory.events are printed then (default:"
  echo "                           the test's cgroup v2 directory under /sys/fs/cgroup)."
  echo "  QORE_TEST_STACK_DUMP_WAIT"
  echo "                           Seconds to wait for those stacks before the test is ended (default: 30)."
  echo "  QORE_TEST_INTERRUPT_WAIT Seconds a test may take to end after a signal ends the run (default: 10); it is"
  echo "                           killed then."
  echo "  CI_NODE_INDEX            Shard index (1-based) for parallel test execution."
  echo "  CI_NODE_TOTAL            Total number of shards for parallel test execution; the shards are balanced by"
  echo "                           the durations of the tests in examples/test/docker_test/test-durations.tsv."
  echo "  QORE_TEST_DURATION_PROFILE"
  echo "                           The column of test-durations.tsv used (default: CI_JOB_NAME without its shard)."
  echo "  QORE_TEST_DURATIONS_FILE The durations file (default: examples/test/docker_test/test-durations.tsv)."
  echo "  QORE_EXCLUDE_PERF_TESTS  Set to '1' to exclude performance tests (same as -E)."
  echo "  QORE_PERF_TESTS_ONLY    Set to '1' to run only performance tests (same as -P), and the tests that"
  echo "                           declare '# test-release-build: <why>', which check a release build (the CI"
  echo "                           test jobs test a debug build; the performance job a release build)."
  echo "  QORE_TEST_EXCLUDE_EXEC_MODE_CHILDREN"
  echo "                           Set to '1' to leave out the tests that declare '# test-exec-mode: children'"
  echo "                           (needs --exec-mode in QORE_TEST_OPTS): their child processes set their own"
  echo "                           execution mode or run in the default mode, so a run in the default mode makes"
  echo "                           the same checks."
  echo "  QORE_TEST_PLATFORM_SUBSET"
  echo "                           Set to 'macos' to run only the tests that declare '# test-platform: macos - <why>'"
  echo "                           (the macOS CI job sets it): the tests whose checks depend on the platform."
  echo "  QORE_TEST_QMOD_DIR       Directory with AOT qmods to prefer before ./qlib."
  echo "  QORE_TEST_SOURCE_MODULES Set to '1' to ignore the default build qmod directory."
  echo "  QORE_TEST_ALLOW_BUILD_MISMATCH"
  echo "                           Set to '1' to run the tests although the build does not match the tree; the run"
  echo "                           says so.  Before anything runs, the commit that the build was made from (qore"
  echo "                           --version's git hash) must be in the tree's repository, and the files the build is"
  echo "                           made from (lib/, include/, qlib/, modules/, cmake/, the C and C++ sources and CMake"
  echo "                           files) must not differ from it, unless the build, configured from this tree, is"
  echo "                           newer than them; changes to tests are allowed.  A tree without git needs"
  echo "                           .git-archive-revision naming the build's commit.  The qcc and the .qmod files the"
  echo "                           tests use must come from the same build.  Otherwise the run ends with a MISMATCH"
  echo "                           error naming both commits and the first files that differ.  The files' times"
  echo "                           do not show a configure without a build, a removed file that was not built"
  echo "                           since, or an include/ header that a module was built with but libqore was not;"
  echo "                           ccache's hard_link or file_clone can make a built change look older (see the"
  echo "                           comment of check_build() in run_tests.sh)."
  echo "  QORE_MODULE_DIR_ONLY     When set, QORE_MODULE_DIR is the complete module path: the directories of the"
  echo "                           installed modules that the build compiled its qmods against (listed by cmake in"
  echo "                           qore-external-module-dirs.txt in the build directory) are not appended to it."
  echo "                           Otherwise they are appended to QORE_MODULE_DIR (those it already holds are not"
  echo "                           added again), so they are searched after it and before the module directories"
  echo "                           compiled into the qore of the build (its installation prefix)."
  echo "  QORE_TEST_BUILD_JOBS     Jobs a test that builds a CMake project runs (default: the number of CPUs, at"
  echo "                           least 2), e.g. examples/test/ir/CMakeBuildHelpers*.qtest."
  echo "  QORE_TEST_PRESERVE_PROVIDER_ENV"
  echo "                           Set to '1' to preserve external provider discovery env vars."
}

err_multiple_format_opts() {
  echo "Multiple formatting options can't be used at the same time." >&2
  print_usage
  exit 1
}

BASE_TEST_PATH="./examples/test"
MEASURE_TIME=0
PRINT_TEXT=1
TEST_DIRS=""
TEST_OUTPUT_FORMAT=""
PERF_EXCLUDE=0
PERF_ONLY=0
LIST_TESTS=0

# Support environment variables for CI integration
if [ "${QORE_EXCLUDE_PERF_TESTS}" = "1" ]; then
    PERF_EXCLUDE=1
fi
if [ "${QORE_PERF_TESTS_ONLY}" = "1" ]; then
    PERF_ONLY=1
fi

while getopts ":d:jlvtEP" opt; do
    case $opt in
        d)
            TEST_DIRS="$TEST_DIRS $BASE_TEST_PATH/$OPTARG"
            ;;
        j)
            if [ -n "$TEST_OUTPUT_FORMAT" ]; then
                err_multiple_format_opts
            else
                TEST_OUTPUT_FORMAT="--format=junit"
                PRINT_TEXT=0
            fi
            ;;
        v)
            if [ -n "$TEST_OUTPUT_FORMAT" ]; then
                err_multiple_format_opts
            else
                TEST_OUTPUT_FORMAT="-v"
            fi
            ;;
        l)
            LIST_TESTS=1
            ;;
        t)
            MEASURE_TIME=1
            ;;
        E)
            PERF_EXCLUDE=1
            ;;
        P)
            PERF_ONLY=1
            ;;
        \?)
            echo "Unknown option: -$OPTARG" >&2
            print_usage
            exit 1
            ;;
        :)
            echo "Option -$OPTARG requires an argument." >&2
            print_usage
            exit 1
            ;;
    esac
done

shift `expr $OPTIND - 1`
# the test files given as arguments, instead of the tests in the test directories
TEST_FILES=""
for test in "$@"; do
    case "$test" in
        *" "*|*"	"*)
            echo "ERROR: a test path cannot contain spaces: '$test'" >&2
            exit 1
            ;;
    esac
    if [ ! -f "$test" ]; then
        echo "ERROR: test '$test' is not a file" >&2
        exit 1
    fi
    TEST_FILES="$TEST_FILES $test"
done

if [ $PERF_EXCLUDE -eq 1 ] && [ $PERF_ONLY -eq 1 ]; then
    echo "Cannot use -E and -P together." >&2
    exit 1
fi

# Test dirs can also be given in the environment, for CI jobs that run a subset of the tests
if [ -z "$TEST_DIRS" ] && [ -n "$QORE_TEST_DIRS" ]; then
    for dir in $QORE_TEST_DIRS; do
        if [ ! -d "$BASE_TEST_PATH/$dir" ]; then
            echo "ERROR: QORE_TEST_DIRS entry '$dir' is not a directory in $BASE_TEST_PATH" >&2
            exit 1
        fi
        TEST_DIRS="$TEST_DIRS $BASE_TEST_PATH/$dir"
    done
fi

# If no test dirs were specified, run all the tests
if [ -z "$TEST_DIRS" ]; then
    TEST_DIRS="$BASE_TEST_PATH"
fi

# Tests outside the test directory, such as those of the binary modules (modules/*/test), are run when named, so
# that the default set of tests stays the same
for dir in $QORE_TEST_EXTRA_DIRS; do
    if [ ! -d "$dir" ]; then
        echo "ERROR: QORE_TEST_EXTRA_DIRS entry '$dir' is not a directory" >&2
        exit 1
    fi
    TEST_DIRS="$TEST_DIRS $dir"
done

# Search for tests in the test directory.
if [ -n "$TEST_FILES" ]; then
    TESTS=$TEST_FILES
else
    TESTS=`find $TEST_DIRS -name "*.qtest" | sort`
fi
FAILED_TESTS=""

# Helper: check if a test matches the perf/stress test pattern
is_perf_test() {
    # the file name, without a subshell for each of the 2000 tests
    case "${1##*/}" in
        *Perf*|PipelineMemory.qtest) return 0 ;;
        *) return 1 ;;
    esac
}

# Filter perf tests if requested
if [ $PERF_EXCLUDE -eq 1 ]; then
    FILTERED=""
    for test in $TESTS; do
        if ! is_perf_test "$test"; then
            FILTERED="$FILTERED $test"
        fi
    done
    TESTS="$FILTERED"
elif [ $PERF_ONLY -eq 1 ]; then
    # with the performance tests, the tests that check a release build (a "# test-release-build: <why>" line, such as
    # the stack budgets of qore/stack/recursion-frame-budget.qtest): the performance job tests CI's only release
    # build, as the test jobs test a debug build, and these tests run there too (issue #6540)
    RELEASE_BUILD_TESTS=""
    if [ -n "$TESTS" ]; then
        # the form that examples/test/runner/test_release_build_markers.py accepts: a reason, without leading or
        # trailing white space
        RELEASE_BUILD_TESTS=" "`grep -lE '^# test-release-build: [^[:space:]](.*[^[:space:]])?$' $TESTS | tr '\n' ' '`
    fi
    FILTERED=""
    for test in $TESTS; do
        if is_perf_test "$test"; then
            FILTERED="$FILTERED $test"
            continue
        fi
        case "$RELEASE_BUILD_TESTS" in
            *" $test "*) FILTERED="$FILTERED $test" ;;
        esac
    done
    TESTS="$FILTERED"
fi

# The tests that declare "# test-exec-mode: children" make all their checks in child processes that set their own
# execution mode (--exec-mode) with ExecModeHarness.qm, so the execution mode of the test itself changes nothing they
# check: a run whose tests all get an execution mode (an --exec-mode option in QORE_TEST_OPTS) leaves them out when
# QORE_TEST_EXCLUDE_EXEC_MODE_CHILDREN=1, as a run in the default mode runs the same child processes.  A harness test
# with checks of its own declares "# test-exec-mode: parent" and always runs.  A test that does not use the harness
# declares "children" when its checks run in child processes of its own binary started without an execution mode,
# which run in the default mode in every job.
# examples/test/runner/test_exec_mode_markers.py checks the declarations.
if [ "${QORE_TEST_EXCLUDE_EXEC_MODE_CHILDREN}" = "1" ]; then
    case " $QORE_TEST_OPTS " in
        *" --exec-mode="*|*" --exec-mode "*) ;;
        *)
            echo "ERROR: QORE_TEST_EXCLUDE_EXEC_MODE_CHILDREN=1 needs an --exec-mode option in QORE_TEST_OPTS: the" \
                "tests it leaves out run only in the default execution mode" >&2
            exit 1
            ;;
    esac
    CHILD_MODE_TESTS=""
    if [ -n "$TESTS" ]; then
        CHILD_MODE_TESTS=`grep -l '^# test-exec-mode: children' $TESTS`
    fi
    FILTERED=""
    EXCLUDED_COUNT=0
    for test in $TESTS; do
        case "
$CHILD_MODE_TESTS
" in
            *"
$test
"*) EXCLUDED_COUNT=$(( EXCLUDED_COUNT + 1 )) ;;
            *) FILTERED="$FILTERED $test" ;;
        esac
    done
    TESTS="$FILTERED"
    if [ $LIST_TESTS -eq 0 ]; then
        echo "Left out $EXCLUDED_COUNT tests that declare 'test-exec-mode: children': their child processes set" \
            "their own execution mode and run in the default-mode jobs"
    fi
fi

# A test whose checks depend on the platform (the OS, its C library, file systems, sockets, processes and signals,
# threads, time zones, locales, dynamic loading, the toolchain and linker) declares each platform beside Linux that it
# must also run on, on a line of its own:
#   # test-platform: macos - <why>
# A run with QORE_TEST_PLATFORM_SUBSET=<platform> runs only the tests that declare that platform; the macOS CI job sets
# it to "macos" (test-macos.sh), as the Linux jobs run every test and so cover the checks that do not depend on the
# platform.  Without QORE_TEST_PLATFORM_SUBSET every test runs and the declarations change nothing.
# examples/test/runner/test_platform_markers.py checks the declarations.
case "${QORE_TEST_PLATFORM_SUBSET}" in
    "") ;;
    macos)
        PLATFORM_TESTS=""
        if [ -n "$TESTS" ]; then
            PLATFORM_TESTS=`grep -l "^# test-platform: ${QORE_TEST_PLATFORM_SUBSET} - " $TESTS`
        fi
        FILTERED=""
        SELECTED_COUNT=0
        ALL_COUNT=0
        for test in $TESTS; do
            ALL_COUNT=$(( ALL_COUNT + 1 ))
            case "
$PLATFORM_TESTS
" in
                *"
$test
"*) SELECTED_COUNT=$(( SELECTED_COUNT + 1 )); FILTERED="$FILTERED $test" ;;
            esac
        done
        TESTS="$FILTERED"
        if [ $LIST_TESTS -eq 0 ]; then
            echo "Selected $SELECTED_COUNT of $ALL_COUNT tests that declare" \
                "'test-platform: ${QORE_TEST_PLATFORM_SUBSET}' (QORE_TEST_PLATFORM_SUBSET): the Linux jobs run the others"
        fi
        ;;
    *)
        echo "ERROR: QORE_TEST_PLATFORM_SUBSET is empty or a platform that tests declare with '# test-platform:'" \
            "(macos): '${QORE_TEST_PLATFORM_SUBSET}'" >&2
        exit 1
        ;;
esac

# The durations of the tests: each CI test job is a column of test-durations.tsv (see update-test-durations.py),
# named by the job without its shard (CI_JOB_NAME "test-alpine-amd64 2/3" reads column test-alpine-amd64), or by
# QORE_TEST_DURATION_PROFILE; another name reads column test-ubuntu-amd64.  A test with no duration counts as the
# median of the others.
TEST_DURATIONS_FILE=${QORE_TEST_DURATIONS_FILE:-$BASE_TEST_PATH/docker_test/test-durations.tsv}
TEST_DURATION_PROFILE=${QORE_TEST_DURATION_PROFILE:-${CI_JOB_NAME% [0-9]*/[0-9]*}}
# the column read: the profile's, else test-ubuntu-amd64; none without a durations file
TEST_DURATION_COLUMN=""
if [ -r "$TEST_DURATIONS_FILE" ]; then
    TEST_DURATION_COLUMN=`awk -F '\t' -v profile="$TEST_DURATION_PROFILE" '
        /^#/ { next }
        $1 == "test" {
            for (i = 2; i <= NF; ++i) {
                if ($i == profile) {
                    found = profile
                }
            }
            print (found != "" ? found : "test-ubuntu-amd64")
            exit
        }' "$TEST_DURATIONS_FILE"`
fi

# Prints "<shard>	<seconds>	<number>	<test>" for each test in TESTS (its number is its place in TESTS), in their
# order (longest first with a second argument), with the shards (1 to the given number)
# that longest-processing-time-first packing gives: the longest test goes to the shard with the least work so far
# (the first such shard on a tie), and tests of the same duration in the order of their names, so that every shard of
# a job computes the same split.  A test counts as at least 0.1 s.
plan_tests() {
    printf '%s\n' $TESTS | awk -v shards="$1" -v order="$2" -v durations="$TEST_DURATIONS_FILE" \
            -v profile="$TEST_DURATION_COLUMN" '
        # sorts idx[lo..hi] by descending duration, then by name
        function before(a, b) {
            return dur[a] > dur[b] || (dur[a] == dur[b] && key[a] < key[b])
        }
        function sort_tests(lo, hi,    i, last, t) {
            if (lo >= hi) {
                return
            }
            t = idx[lo]; idx[lo] = idx[int((lo + hi) / 2)]; idx[int((lo + hi) / 2)] = t
            last = lo
            for (i = lo + 1; i <= hi; ++i) {
                if (before(idx[i], idx[lo])) {
                    ++last
                    t = idx[last]; idx[last] = idx[i]; idx[i] = t
                }
            }
            t = idx[lo]; idx[lo] = idx[last]; idx[last] = t
            sort_tests(lo, last - 1)
            sort_tests(last + 1, hi)
        }
        function sort_values(lo, hi,    i, last, t) {
            if (lo >= hi) {
                return
            }
            t = val[lo]; val[lo] = val[int((lo + hi) / 2)]; val[int((lo + hi) / 2)] = t
            last = lo
            for (i = lo + 1; i <= hi; ++i) {
                if (val[i] < val[lo]) {
                    ++last
                    t = val[last]; val[last] = val[i]; val[i] = t
                }
            }
            t = val[lo]; val[lo] = val[last]; val[last] = t
            sort_values(lo, last - 1)
            sort_values(last + 1, hi)
        }
        BEGIN {
            # the number of tests, a number from the start: an unassigned variable is "" as a subscript
            n = 0
            col = 0
            while ((getline line < durations) > 0) {
                if (line ~ /^#/ || line == "") {
                    continue
                }
                nf = split(line, f, "\t")
                if (f[1] == "test") {
                    for (i = 2; i <= nf; ++i) {
                        if (f[i] == profile) {
                            col = i
                        }
                    }
                    continue
                }
                if (col && f[col] != "" && f[col] != "-") {
                    known[f[1]] = f[col] + 0
                }
            }
        }
        {
            test[n] = $0
            key[n] = $0
            sub(/^\.\//, "", key[n])
            ++n
        }
        END {
            nv = 0
            for (i = 0; i < n; ++i) {
                if (key[i] in known) {
                    val[nv++] = known[key[i]]
                }
            }
            sort_values(0, nv - 1)
            median = nv ? (nv % 2 ? val[int(nv / 2)] : (val[nv / 2 - 1] + val[nv / 2]) / 2) : 1
            # a test takes at least 0.1 s (the durations have that precision), so that the shortest tests are
            # spread over the shards too
            for (i = 0; i < n; ++i) {
                dur[i] = (key[i] in known) ? known[key[i]] : median
                if (dur[i] < 0.1) {
                    dur[i] = 0.1
                }
                idx[i] = i
            }
            sort_tests(0, n - 1)
            for (s = 1; s <= shards; ++s) {
                load[s] = 0
            }
            for (j = 0; j < n; ++j) {
                i = idx[j]
                best = 1
                for (s = 2; s <= shards; ++s) {
                    if (load[s] < load[best]) {
                        best = s
                    }
                }
                shard[i] = best
                load[best] += dur[i]
            }
            for (j = 0; j < n; ++j) {
                i = order ? idx[j] : j
                printf "%d\t%s\t%d\t%s\n", shard[i], dur[i], i + 1, test[i]
            }
        }'
}

# Shard tests for parallel CI execution: GitLab sets CI_NODE_INDEX (1-based) and CI_NODE_TOTAL with parallel: N; the
# shards are balanced by the durations of the tests (see plan_tests())
TEST_PLAN=""
SHARD_SUMMARY_FD=1
if [ $LIST_TESTS -eq 1 ]; then
    SHARD_SUMMARY_FD=2
fi
if [ -n "$CI_NODE_INDEX" ] && [ -n "$CI_NODE_TOTAL" ] && [ "$CI_NODE_TOTAL" -gt 1 ] 2>/dev/null; then
    if [ "$CI_NODE_INDEX" -lt 1 ] 2>/dev/null || [ "$CI_NODE_INDEX" -gt "$CI_NODE_TOTAL" ] 2>/dev/null; then
        echo "ERROR: CI_NODE_INDEX $CI_NODE_INDEX is not a shard of $CI_NODE_TOTAL" >&2
        exit 1
    fi
    TEST_PLAN=`plan_tests "$CI_NODE_TOTAL"`
    TESTS=`printf '%s\n' "$TEST_PLAN" | awk -F '\t' -v shard="$CI_NODE_INDEX" '$1 == shard { print $4 }'`
    # on stderr with -l, which lists only the tests on stdout
    if [ $PRINT_TEXT -eq 1 ]; then
        printf '%s\n' "$TEST_PLAN" | awk -F '\t' -v shard="$CI_NODE_INDEX" -v total="$CI_NODE_TOTAL" \
            -v column="${TEST_DURATION_COLUMN:-no durations file}" '
            { all += $2; if ($1 == shard) { mine += $2; ++n } }
            END { printf "Shard %d/%d selected: %d tests, %.0fs of %.0fs by the durations of %s\n", shard, total, n,
                  mine, all, column }' >&$SHARD_SUMMARY_FD
    fi
fi

TEST_COUNT=`echo $TESTS | wc -w`
TEST_COUNT=`expr $TEST_COUNT + 0`
# -l: the tests that would run, one per line, and nothing else
if [ $LIST_TESTS -eq 1 ]; then
    if [ $TEST_COUNT -eq 0 ]; then
        echo "ERROR: no tests found to run" >&2
        exit 1
    fi
    for test in $TESTS; do
        echo "$test"
    done
    exit 0
fi

QORE=""
QR=""
LIBQORE=""
QORE_IS_BUILD_TREE=0
QORE_LIB_PATH="./lib/.libs:./qlib:$LD_LIBRARY_PATH"

set_qore_build_dir () {
    d="$1"
    if [ ! -f "$d/CMakeCache.txt" ] || [ ! -f "$d/qore" ]; then
        return 1
    fi
    if [ -f "$d/libqore.so" ]; then
        LIBQORE="$d/libqore.so"
    elif [ -f "$d/libqore.dylib" ]; then
        LIBQORE="$d/libqore.dylib"
    else
        return 1
    fi
    QORE="$d/qore"
    QR=""
    QORE_IS_BUILD_TREE=1
    if [ -f "$d/qr" ]; then
        QR="$d/qr"
    fi
    return 0
}

# Allow callers to override binaries (ex: debug build output).
if [ -n "$QORE_BINARY" ]; then
    QORE="$QORE_BINARY"
    QORE_DIR=`dirname "$QORE"`
    if [ -f "$QORE_DIR/libqore.so" ]; then
        LIBQORE="$QORE_DIR/libqore.so"
    elif [ -f "$QORE_DIR/libqore.dylib" ]; then
        LIBQORE="$QORE_DIR/libqore.dylib"
    fi
    if [ -f "$QORE_DIR/CMakeCache.txt" ]; then
        QORE_IS_BUILD_TREE=1
    fi
    if [ -f "$QORE_DIR/qr" ]; then
        QR="$QORE_DIR/qr"
    fi
fi

# Allow explicit override for libqore if needed.
if [ -n "$LIBQORE_BINARY" ]; then
    LIBQORE="$LIBQORE_BINARY"
fi

# Test that qore is built (CMake only, autotools no longer supported).
if [ -z "$QORE" ]; then
    if [ -d "build" ]; then
        set_qore_build_dir "build"
    fi
fi
if [ -z "$QORE" ]; then
    for D in `ls -d */`; do
        d=`echo ${D%%/}`
        if [ "$d" = "build" ]; then
            continue
        fi
        if set_qore_build_dir "$d"; then
            break
        fi
    done
fi

if [ -n "$QORE_BINARY" ] && [ -z "$LIBQORE" ]; then
    # a QORE_BINARY that is a wrapper script (or an installed qore) has no libqore next to it
    echo "QORE_BINARY=$QORE_BINARY has no libqore next to it; set LIBQORE_BINARY=<build>/libqore.so when" \
        "QORE_BINARY is a wrapper. Exiting."
    exit 1
fi
if [ -z "$QORE" ] || [ -z "$LIBQORE" ]; then
    echo "Qore is not built. Exiting."
    exit 1
fi

export QORE_BINARY="$QORE"
export LIBQORE_BINARY="$LIBQORE"

QORE_DIR=`dirname "$QORE"`
QORE_BUILD_DIR="$QORE_DIR"
if [ -n "$QORE_LIBDIR" ] && [ -d "$QORE_LIBDIR" ]; then
    QORE_BUILD_DIR="$QORE_LIBDIR"
elif [ -n "$LIBQORE" ]; then
    _libqore_dir=`dirname "$LIBQORE"`
    if [ -d "$_libqore_dir/qlib-qmod" ] || [ -d "$_libqore_dir/modules" ]; then
        QORE_BUILD_DIR="$_libqore_dir"
    elif [ -d "$_libqore_dir/../qlib-qmod" ] || [ -d "$_libqore_dir/../modules" ]; then
        QORE_BUILD_DIR="$_libqore_dir/.."
    fi
fi
QORE_SOURCE_DIR=`pwd -P`
QORE_BUILD_DIR=`cd "$QORE_BUILD_DIR" && pwd -P`
BUILD_MODULE_DIRS=""
BUILD_QMOD_DIR=""
if [ -d "$QORE_BUILD_DIR/modules" ]; then
    for moddir in "$QORE_BUILD_DIR/modules"/*; do
        if [ -d "$moddir" ]; then
            if [ -z "$BUILD_MODULE_DIRS" ]; then
                BUILD_MODULE_DIRS="$moddir"
            else
                BUILD_MODULE_DIRS="$BUILD_MODULE_DIRS:$moddir"
            fi
        fi
    done
fi
if [ -n "$QORE_TEST_QMOD_DIR" ]; then
    if [ ! -d "$QORE_TEST_QMOD_DIR" ]; then
        echo "QORE_TEST_QMOD_DIR does not exist or is not a directory: $QORE_TEST_QMOD_DIR"
        exit 1
    fi
    BUILD_QMOD_DIR="$QORE_TEST_QMOD_DIR"
elif [ "$QORE_TEST_SOURCE_MODULES" != "1" ] && [ -d "$QORE_DIR/qlib-qmod" ]; then
    BUILD_QMOD_DIR="$QORE_DIR/qlib-qmod"
elif [ "$QORE_TEST_SOURCE_MODULES" != "1" ] && [ -d "$QORE_BUILD_DIR/qlib-qmod" ]; then
    BUILD_QMOD_DIR="$QORE_BUILD_DIR/qlib-qmod"
fi
if [ -n "$BUILD_QMOD_DIR" ]; then
    BUILD_QMOD_DIR=`cd "$BUILD_QMOD_DIR" && pwd -P`
fi

# The timeout command (see the time limit of a test below): gtimeout on macOS (GNU coreutils) where there is no timeout
if command -v timeout >/dev/null 2>&1; then
    TIMEOUT_CMD=timeout
elif command -v gtimeout >/dev/null 2>&1; then
    TIMEOUT_CMD=gtimeout
else
    TIMEOUT_CMD=""
fi

# The build must match the tree (qore#6897): the tests and the qlib sources of one tree run with the qore, libqore, qcc
# and .qmod files of another build produced failures that looked like anything but a mismatch.  Before any test runs
# (check_build(), once the library path of the tests is set up, so that each program finds the libqore that it finds
# for the tests), the commit that the build was made from (the "git hash" of qore --version, run through the qore that
# the tests use) is compared with the tree:
# - in a git working tree, the commit must be in the tree's repository, and the files that the build is made from
#   (BUILD_INPUT_PATHSPECS: libqore, the programs, the modules and the CMake files; not the tests, the documentation or
#   the scripts) must not differ between that commit and the working tree.  When the build was configured from this
#   very tree (its CMakeCache.txt names it), a file that differs is accepted if what the tests use that the build makes
#   from it is newer than it (build_references()): the build then has it (an uncommitted change that was built, or a
#   commit made after the build), while a file changed after the build (an edit or a checkout that was not built) is
#   not.  A file that the build makes nothing from that the tests use (a source under examples/ that a test compiles
#   when it runs, a file of a binary module that CMake does not build here, a module in qlib that has no .qmod, which
#   is loaded from its source, a file that is not a source) is not compared, and neither is a file that is gone.  A
#   new file that git does not know of is not
#   seen.  The times tell what a build has only as far as the build writes what the tests use again, so these cases
#   are not seen, and QORE_TEST_ALLOW_BUILD_MISMATCH=1 runs one that is refused wrongly:
#   - a configure that is not followed by a build hides a change of a CMake file (CMakeCache.txt is written);
#   - a file removed from libqore, a module in qlib, a binary module or the test programs and not built since;
#   - a header of include/ counts as built when anything that includes such headers was built after it (libqore, qcc,
#     qore, a binary module), so an old libqore is accepted when a module that includes the header was built again;
#     a header that an example module includes too is not compared with the example module;
#   - a source of a test program is compared with each object compiled from it, also that of a helper target that was
#     built once and that a normal build does not make again, which is then refused until that target is built;
#   - an object file that ccache made a hard link or a clone of (hard_link, file_clone) has the time of the cached
#     object, which may be older than a change that was built, so that the change is refused.
# - in a tree without git (an archive), .git-archive-revision must name the commit (an archive that git makes has
#   it, see .gitattributes), and it must be the build's;
# - the qcc that the tests use (QCC, QORE_QCC, QORE_QCC_EXECUTABLE, the qcc next to qore, or the one in the PATH; see
#   examples/test/TestInterpreter.qm) and the .qmod files that they load (the build's qlib-qmod or QORE_TEST_QMOD_DIR,
#   and the build that each qlib/*.qmod symlink points into) must come from the same build: the qore next to each of
#   them must be the qore used or name the same commit.
# A mismatch ends the run with a MISMATCH error naming both commits and the first paths that differ, unless
# QORE_TEST_ALLOW_BUILD_MISMATCH=1, with which the run goes on and says so.  Each program and git command it runs is
# limited to BUILD_CHECK_WAIT seconds by the timeout command (a libqore that hangs in its initialization cannot hold up
# the run), and git never fetches (GIT_NO_LAZY_FETCH: a partial clone would fetch a missing object from the network).

# The files that the build is made from, as git pathspecs from the root of the tree (decided from the inputs of
# CMakeLists.txt): not the tests and the documentation of the binary modules; under examples/, the C and C++ sources and
# CMake files, of which only those that the build compiles count (see build_references())
BUILD_INPUT_PATHSPECS="lib include qlib modules cmake CMakeLists.txt :(glob)*.cpp :(glob)*.c :(glob)*.h :(glob)*.in
:(glob)examples/**/*.cpp :(glob)examples/**/*.cc :(glob)examples/**/*.c :(glob)examples/**/*.h :(glob)examples/**/*.hpp
:(glob)examples/**/*.qpp :(glob)examples/**/CMakeLists.txt :(glob)examples/**/*.cmake :(exclude,glob)modules/*/test/**
:(exclude,glob)modules/*/docs/**"
BUILD_CHECK_WAIT=${QORE_TEST_BUILD_CHECK_WAIT:-60}

# Runs the given command, limited to BUILD_CHECK_WAIT seconds where there is a timeout command
run_build_check() {
    if [ -n "$TIMEOUT_CMD" ]; then
        $TIMEOUT_CMD -s KILL $BUILD_CHECK_WAIT "$@"
    else
        "$@"
    fi
}

# Prints the commit that the given qore was built from (its --version's "git hash", which libqore holds), or nothing
# if it names none.  The qore that the tests use runs with their library path.  Another qore (the second argument is
# 1: the one next to a qcc or to .qmod files) runs with the libqore next to it first in the library path, if it has one,
# as its build's tests would, or else without LD_LIBRARY_PATH, so that it loads the libqore that its own loader finds
# (an installed one): with the tests' path, which comes before its RUNPATH on Linux, it would load the build's libqore
# and name the build's commit.  It runs in a subshell, so that the shell's own LD_LIBRARY_PATH is left as it is.
build_git_hash() {
    (
        if [ "$2" = 1 ]; then
            hash_dir=`dirname "$1"`
            if [ -f "$hash_dir/libqore.so" ] || [ -f "$hash_dir/libqore.dylib" ]; then
                LD_LIBRARY_PATH="$hash_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
                export LD_LIBRARY_PATH
            else
                unset LD_LIBRARY_PATH
            fi
        fi
        run_build_check "$1" --version 2>/dev/null
    ) | sed -n 's/^[	 ]*git hash:[	 ]*\([0-9a-f]\{40,64\}\)[	 ]*$/\1/p' | head -1
}

# Runs git in the tree; safe.directory: a tree that another user owns (a CI job's checkout) is read all the same.
# The commands read the build's commit and its files only, which a shallow clone of that commit has (see
# examples/test/runner/test_ci_shallow_clone.py).
tree_git() {
    # env: "VAR=value function" would leave the variable set (and exported) in bash
    run_build_check env GIT_NO_LAZY_FETCH=1 git -c safe.directory='*' -C "$QORE_SOURCE_DIR" "$@"
}

# Prints the physical path of the given directory, or nothing
physical_dir() {
    (cd "$1" 2>/dev/null && pwd -P)
}

BUILD_MISMATCH=""
# Adds a line to the description of the mismatch
build_mismatch() {
    BUILD_MISMATCH="$BUILD_MISMATCH  $*
"
}

# Checks that the qore next to the given file (qcc, or a .qmod directory: the first argument names it) is the build's,
# or names the build's commit
check_companion_build() {
    companion_dir=`physical_dir "$2"`
    if [ -n "$companion_dir" ] && [ "$companion_dir" = "$QORE_BUILD_DIR" ]; then
        return
    fi
    if [ -n "$companion_dir" ] && [ -x "$companion_dir/qore" ]; then
        companion_hash=`build_git_hash "$companion_dir/qore" 1`
        if [ "$companion_hash" = "$BUILD_HASH" ]; then
            return
        fi
        build_mismatch "$1: from the build in $companion_dir, made from commit ${companion_hash:-unknown}, not" \
            "$BUILD_HASH"
        return
    fi
    build_mismatch "$1: from $2, which has no qore that names its commit: they cannot be told to be the build's"
}

# Adds each of the given files that exists to BUILD_REFS, one per line (a pattern of the shell that matches nothing
# stays as it is, and does not exist)
add_build_refs() {
    for build_ref in "$@"; do
        if [ -e "$build_ref" ]; then
            case "
$BUILD_REFS" in
                *"
$build_ref
"*) ;;
                *) BUILD_REFS="$BUILD_REFS$build_ref
" ;;
            esac
        fi
    done
}

# Adds to BUILD_REFS the object files that the build compiled from the given source file under examples/ (CMake writes
# them in the build directory of the source's CMake directory, as <that directory>/CMakeFiles/<target>.dir/<source>.o,
# so each directory that contains the source is tried), or, with a second argument, those compiled from the sources of
# the given directory itself (not of its subdirectories)
add_build_objects() {
    object_prefix=""
    object_rest=$1
    while :; do
        object_dir=$QORE_BUILD_DIR/${object_prefix:+$object_prefix/}CMakeFiles
        if [ -z "$2" ]; then
            add_build_refs "$object_dir/"*.dir/"$object_rest.o"
        elif [ -n "$object_rest" ]; then
            add_build_refs "$object_dir/"*.dir/"$object_rest"/*.o
        else
            add_build_refs "$object_dir/"*.dir/*.o
            return
        fi
        case "$object_rest" in
            */*)
                object_prefix=${object_prefix:+$object_prefix/}${object_rest%%/*}
                object_rest=${object_rest#*/}
                ;;
            *)
                if [ -z "$2" ]; then
                    return
                fi
                object_prefix=${object_prefix:+$object_prefix/}$object_rest
                object_rest=""
                ;;
        esac
    done
}

# Adds to BUILD_REFS the object files of the targets of the given directory under examples/ (an example module, built
# by add_subdirectory()): those in its build directory, in its target directories up to two levels deep
add_module_objects() {
    object_dir=$QORE_BUILD_DIR/$1/CMakeFiles
    add_build_refs "$object_dir/"*.dir/*.o "$object_dir/"*.dir/*/*.o "$object_dir/"*.dir/*/*/*.o
}

# Prints the directory of the example module of the given file under examples/: the nearest directory, from the file's
# own up to examples/, whose CMakeLists.txt defines a target; nothing if there is none
example_module_dir() {
    module_dir=${1%/*}
    while [ "$module_dir" != examples ] && [ -n "$module_dir" ]; do
        if [ -f "$QORE_SOURCE_DIR/$module_dir/CMakeLists.txt" ] \
                && grep -Eq '^[	 ]*(add_library|add_executable|qore_binary_module|qore_external_binary_module)[	 ]*\(' \
                "$QORE_SOURCE_DIR/$module_dir/CMakeLists.txt" 2>/dev/null; then
            echo "$module_dir"
            return
        fi
        case "$module_dir" in
            */*) module_dir=${module_dir%/*} ;;
            *) return ;;
        esac
    done
}

# Prints the .qmod that the tests load for the given module in qlib, in either layout (qlib/<name>.qmod for a module
# in one file, qlib/<name>/<name>.qmod for a separated one; see cmake/QoreMacros.cmake): the build's, or the one that
# qlib links to; nothing if there is none (the module is loaded from its source)
qlib_module_qmod() {
    for qmod in "$BUILD_QMOD_DIR/$1.qmod" "$BUILD_QMOD_DIR/$1/$1.qmod" "$QORE_SOURCE_DIR/qlib/$1.qmod" \
            "$QORE_SOURCE_DIR/qlib/$1/$1.qmod"; do
        case "$qmod" in
            /*) ;;
            *) continue ;;
        esac
        if [ -e "$qmod" ]; then
            echo "$qmod"
            return
        fi
    done
}

# Sets BUILD_REFS to the files that the tests use that the build makes from the given changed file (relative to the
# tree), one per line, whose times tell whether the build has the change (see above), or to nothing if the tests use
# nothing that the build makes from it.  BUILD_REFS_ANY is 1 if the build has the change when the file is older than
# any of them, and 0 if it must be older than all.  Each is found with the shell's own commands, so that a diff of many
# files costs little.  Only a file of a kind that a target compiles is compared, and only with what the tests use,
# never with an object of a helper target that a normal build does not make again (such as one that compiles libqore's
# sources once more), which would keep a file refused for good:
# - a CMake file: CMakeCache.txt, which CMake writes when it configures the build again; under examples/, only one in
#   a directory that CMake has processed;
# - a source of a module in qlib (.qm, .qc; not its other files, which the .qmod does not hold): the module's .qmod
#   (see qlib_module_qmod()), or nothing (it is loaded from its source);
# - a source of libqore (lib/: C, C++, qpp, bison, flex, assembler and headers): libqore, but nothing for a C or C++
#   source that a build of this tree with an object for each source (not one compilation unit) did not compile into
#   it (a file of another platform, or a program's own);
# - a header of include/: any of libqore, qcc, qore and the binary modules' files (BUILD_REFS_ANY): a header that only
#   modules include is in a module that was built again, not in libqore;
# - a source of a binary module (modules/<name>/: C, C++, qpp, headers, Qore): the module's file, or nothing if the
#   build has not built it;
# - a C or C++ source at the root of the tree: the program it is part of (qcc, qr, qore);
# - a file under examples/: the objects compiled from it (a test program's source: a helper target's object, if one was
#   built once, is compared too), or, for a header or a qpp file, the objects compiled from the sources of its own
#   directory and those of its example module's targets (see example_module_dir()), never of a directory above; or
#   nothing (a source that a test compiles when it runs, which CMake does not compile).
build_references() {
    BUILD_REFS=""
    BUILD_REFS_ANY=0
    case "$1" in
        examples/*/CMakeLists.txt|examples/*.cmake)
            # only one that CMake has processed (a build directory of its own): a test compiles some fixtures with a
            # CMakeLists.txt of their own when it runs
            if [ -d "$QORE_BUILD_DIR/${1%/*}/CMakeFiles" ]; then
                add_build_refs "$QORE_BUILD_DIR/CMakeCache.txt"
            fi
            ;;
        CMakeLists.txt|*/CMakeLists.txt|*.cmake|cmake/*|*.in)
            add_build_refs "$QORE_BUILD_DIR/CMakeCache.txt"
            ;;
        qlib/*.qm|qlib/*.qc)
            ref_module=${1#qlib/}
            ref_module=${ref_module%%/*}
            ref_module=${ref_module%.qm}
            ref_module=${ref_module%.qc}
            ref_qmod=`qlib_module_qmod "$ref_module"`
            if [ -n "$ref_qmod" ]; then
                add_build_refs "$ref_qmod"
            fi
            ;;
        lib/*.c|lib/*.cc|lib/*.cpp)
            # In a build of this tree with an object for each source (not one compilation unit, CMakeCache.txt's
            # SINGLE_COMPILATION_UNIT, which is on by default and keeps a few objects of their own), a source that
            # has none is not in libqore (a file of another platform, or a program's own).  Another tree's build is
            # compared for every source.
            if [ "$same_tree" = 1 ] && [ "$BUILD_SCU" = 0 ] \
                    && [ ! -e "$QORE_BUILD_DIR/CMakeFiles/libqore.dir/$1.o" ]; then
                return
            fi
            add_build_refs "$LIBQORE"
            ;;
        lib/*.h|lib/*.hpp|lib/*.qpp|lib/*.ypp|lib/*.lpp|lib/*.y|lib/*.l|lib/*.s|lib/*.S|lib/*.inc)
            add_build_refs "$LIBQORE"
            ;;
        include/*)
            BUILD_REFS_ANY=1
            add_build_refs "$LIBQORE" "$QORE_BUILD_DIR/qcc" "$QORE_BUILD_DIR/qore" "$QORE_BUILD_DIR/modules/"*/*.qmod
            ;;
        modules/*.c|modules/*.cc|modules/*.cpp|modules/*.h|modules/*.hpp|modules/*.qpp|modules/*.qm|modules/*.qc)
            ref_module=${1#modules/}
            ref_module=${ref_module%%/*}
            add_build_refs "$QORE_BUILD_DIR/modules/$ref_module/"*.qmod
            ;;
        examples/*)
            add_build_objects "$1"
            case "$1" in
                *.h|*.hpp|*.qpp)
                    if [ -z "$BUILD_REFS" ]; then
                        BUILD_REFS_ANY=1
                        add_build_objects "${1%/*}" directory
                        ref_module_dir=`example_module_dir "$1"`
                        if [ -n "$ref_module_dir" ]; then
                            add_module_objects "$ref_module_dir"
                        fi
                    fi
                    ;;
            esac
            ;;
        qcc-main.cpp) add_build_refs "$QORE_BUILD_DIR/qcc" ;;
        qr-*.cpp) add_build_refs "$QORE_BUILD_DIR/qr" ;;
        *-single-compilation-unit.cpp) ;;
        *.c|*.cc|*.cpp|*.h)
            add_build_refs "$QORE_BUILD_DIR/qore"
            if [ -z "$BUILD_REFS" ]; then
                add_build_refs "$LIBQORE"
            fi
            ;;
    esac
}

# Returns 0 if the build has the change of the given file (or directory, for a qlib file that is gone) by the times of the
# files in BUILD_REFS (see build_references())
build_has_change() {
    build_newer=0
    build_older=0
    while IFS= read -r build_ref; do
        if [ -z "$build_ref" ]; then
            continue
        fi
        if [ "$1" -nt "$build_ref" ]; then
            build_newer=1
        else
            build_older=1
        fi
    done <<BUILD_REFS_EOF
$BUILD_REFS
BUILD_REFS_EOF
    if [ $BUILD_REFS_ANY -eq 1 ]; then
        [ $build_older -eq 1 ]
    else
        [ $build_newer -eq 0 ]
    fi
}

check_build_matches_tree() {
    BUILD_HASH=`build_git_hash "$QORE"`
    BUILD_TREE_HASH=""
    if [ -z "$BUILD_HASH" ]; then
        build_mismatch "the build ($QORE) does not name the commit it was made from (qore --version shows no git hash)"
        return
    fi
    if [ -e "$QORE_SOURCE_DIR/.git" ]; then
        if ! command -v git > /dev/null 2>&1; then
            build_mismatch "the tree is a git working tree, but git is not installed to compare it with the build"
            return
        fi
        BUILD_TREE_HASH=`tree_git rev-parse --verify -q "HEAD^{commit}" 2>/dev/null`
        if ! tree_git cat-file -e "$BUILD_HASH^{commit}" 2>/dev/null; then
            build_mismatch "the build's commit $BUILD_HASH is not in the tree's repository (the tree is at" \
                "${BUILD_TREE_HASH:-an unknown commit})"
            return
        fi
        # set -f: the pathspecs are not file name patterns for the shell; git's messages (a warning on a diff that
        # works) are no file names: they are shown only if the diff fails
        build_errors=`mktemp "${TMPDIR:-/tmp}/qore-build-check.XXXXXX"` || build_errors=/dev/null
        set -f
        build_changes=`tree_git diff --name-only --no-renames "$BUILD_HASH" -- $BUILD_INPUT_PATHSPECS 2>"$build_errors"`
        build_diff_rc=$?
        set +f
        build_diff_errors=`cat "$build_errors" 2>/dev/null`
        if [ "$build_errors" != /dev/null ]; then
            rm -f "$build_errors"
        fi
        if [ $build_diff_rc -ne 0 ]; then
            build_mismatch "the tree cannot be compared with the build's commit $BUILD_HASH: $build_diff_errors"
            return
        fi
        if [ -z "$build_changes" ]; then
            return
        fi
        # a build configured from this tree: the files that it is newer than are in it
        build_source=`sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$QORE_BUILD_DIR/CMakeCache.txt" 2>/dev/null`
        same_tree=0
        BUILD_SCU=1
        if grep -Eiq '^SINGLE_COMPILATION_UNIT:BOOL=(OFF|FALSE|NO|N|0)[[:space:]]*$' "$QORE_BUILD_DIR/CMakeCache.txt" \
                2>/dev/null; then
            BUILD_SCU=0
        fi
        if [ -n "$build_source" ] && [ "`physical_dir "$build_source"`" = "$QORE_SOURCE_DIR" ]; then
            same_tree=1
        fi
        stale=""
        stale_count=0
        while IFS= read -r path; do
            if [ -z "$path" ]; then
                continue
            fi
            # A file that is gone, in a build of this tree, is not compared (a directory's time is no build's: an
            # editor's file changes it).  Another build that has a file that the tree no longer has differs, if the
            # tests use what it makes from it.
            file=$QORE_SOURCE_DIR/$path
            if [ ! -e "$file" ] && [ $same_tree -eq 1 ]; then
                continue
            fi
            build_references "$path"
            if [ -z "$BUILD_REFS" ]; then
                continue
            fi
            if [ $same_tree -eq 1 ] && build_has_change "$file"; then
                continue
            fi
            stale_count=$((stale_count + 1))
            stale="$stale    $path
"
            # the first ten are enough to tell
            if [ $stale_count -ge 10 ]; then
                stale_count="10 or more"
                break
            fi
        done <<BUILD_CHANGES_EOF
$build_changes
BUILD_CHANGES_EOF
        if [ -n "$stale" ]; then
            if [ $same_tree -eq 1 ]; then
                build_mismatch "$stale_count file(s) of the build changed since the build was made (from commit" \
                    "$BUILD_HASH; the tree is at $BUILD_TREE_HASH); the first of them:"
            else
                build_mismatch "$stale_count file(s) of the build differ between the build's commit $BUILD_HASH" \
                    "and the tree (at $BUILD_TREE_HASH), and the build was not made from this tree; the first of them:"
            fi
            BUILD_MISMATCH="$BUILD_MISMATCH$stale"
        fi
    else
        marker=`grep -v '^#' "$QORE_SOURCE_DIR/.git-archive-revision" 2>/dev/null | head -1 | tr -d ' \t\r'`
        case "$marker" in
            *[!0-9a-f]*|"")
                build_mismatch "the tree has no git repository and no .git-archive-revision naming the commit it was" \
                    "made from (an archive that git makes has it), so it cannot be compared with the build's commit" \
                    "$BUILD_HASH"
                ;;
            *)
                BUILD_TREE_HASH=$marker
                if [ "$marker" != "$BUILD_HASH" ]; then
                    build_mismatch "the tree was made from commit $marker (.git-archive-revision), the build from" \
                        "$BUILD_HASH"
                fi
                ;;
        esac
    fi
}

# Checks the qcc and the .qmod files that the tests use (see above)
check_build_parts() {
    if [ -z "$BUILD_HASH" ]; then
        return
    fi
    test_qcc=""
    for candidate in "$QCC" "$QORE_QCC" "$QORE_QCC_EXECUTABLE"; do
        if [ -n "$candidate" ]; then
            test_qcc=$candidate
            break
        fi
    done
    if [ -z "$test_qcc" ]; then
        if [ -f "$QORE_DIR/qcc" ]; then
            test_qcc=$QORE_DIR/qcc
        else
            test_qcc=`command -v qcc 2>/dev/null`
        fi
    fi
    case "$test_qcc" in
        */*) check_companion_build "the qcc that the tests use ($test_qcc)" "`dirname "$test_qcc"`" ;;
    esac
    if [ -n "$BUILD_QMOD_DIR" ]; then
        check_companion_build "the .qmod files in $BUILD_QMOD_DIR" "`dirname "$BUILD_QMOD_DIR"`"
    fi
    # the builds that the tree's links to .qmod files point into, each once: qlib/<name>.qmod links to
    # <build>/qlib-qmod/<name>.qmod, and qlib/<name>/<name>.qmod (a separated module) to
    # <build>/qlib-qmod/<name>/<name>.qmod; ls prints each link and its target, after "->" (BusyBox's readlink takes one
    # file)
    qmod_link_dirs=`find "$QORE_SOURCE_DIR/qlib" -maxdepth 2 -name '*.qmod' -type l -exec ls -ld {} + 2>/dev/null \
        | while IFS= read -r qmod_line; do
            qmod_link=${qmod_line% -> *}
            qmod_target=${qmod_line##* -> }
            case "$qmod_link" in
                */qlib/*/*.qmod)
                    # relative to the module's directory in qlib; the build is three levels up
                    qmod_base=${qmod_link##*/}
                    qmod_base=$QORE_SOURCE_DIR/qlib/${qmod_base%.qmod}
                    qmod_build=${qmod_target%/*/*/*}
                    ;;
                *)
                    qmod_base=$QORE_SOURCE_DIR/qlib
                    qmod_build=${qmod_target%/*/*}
                    ;;
            esac
            case "$qmod_build" in
                /*) echo "$qmod_build" ;;
                *) echo "$qmod_base/$qmod_build" ;;
            esac
        done | sort -u`
    if [ -n "$qmod_link_dirs" ]; then
        while IFS= read -r qmod_build; do
            check_companion_build "the qlib .qmod links into $qmod_build" "$qmod_build"
        done <<QMOD_LINKS_EOF
$qmod_link_dirs
QMOD_LINKS_EOF
    fi
}

# Checks that the build matches the tree (see above); ends the run on a mismatch, unless it is allowed
check_build() {
    check_build_matches_tree
    check_build_parts
    if [ -z "$BUILD_MISMATCH" ]; then
        return
    fi
    echo "*** MISMATCH: the build does not match the tree: the tests would run with a build made from other" \
        "sources (build: $QORE, commit ${BUILD_HASH:-unknown}; tree: $QORE_SOURCE_DIR, commit" \
        "${BUILD_TREE_HASH:-unknown}):"
    printf '%s' "$BUILD_MISMATCH"
    if [ "$QORE_TEST_ALLOW_BUILD_MISMATCH" = 1 ]; then
        echo "*** the tests run all the same: QORE_TEST_ALLOW_BUILD_MISMATCH=1 ***"
    else
        echo "*** rebuild, or test the tree the build was made from; QORE_TEST_ALLOW_BUILD_MISMATCH=1 runs the tests" \
            "all the same. Exiting. ***"
        exit 1
    fi
}

export LD_LIBRARY_PATH=$QORE_LIB_PATH
TEST_MODULE_DIRS=""
if [ -n "$BUILD_QMOD_DIR" ]; then
    TEST_MODULE_DIRS="$BUILD_QMOD_DIR"
fi
if [ -n "$BUILD_MODULE_DIRS" ]; then
    if [ -n "$TEST_MODULE_DIRS" ]; then
        TEST_MODULE_DIRS="$TEST_MODULE_DIRS:$BUILD_MODULE_DIRS"
    else
        TEST_MODULE_DIRS="$BUILD_MODULE_DIRS"
    fi
fi
if [ -n "$TEST_MODULE_DIRS" ]; then
    export QORE_MODULE_DIR=$TEST_MODULE_DIRS:$QORE_SOURCE_DIR/qlib:$QORE_MODULE_DIR
else
    export QORE_MODULE_DIR=$QORE_SOURCE_DIR/qlib:$QORE_MODULE_DIR
fi
# The directories of the installed modules that the build compiled its qlib qmods against (written by cmake): a qmod
# requires the modules it was compiled with, and the build's qore does not search these directories when they are
# outside its prefix, so without them the qmod falls back to its source (BINARY-MODULE-SOURCE-FALLBACK).  They come
# last in QORE_MODULE_DIR, so the modules of the build and of this tree come first; qore searches QORE_MODULE_DIR before
# the module directories compiled into it, so they also come before those of its installation prefix.  A directory
# that QORE_MODULE_DIR already holds is not added again.  With QORE_MODULE_DIR_ONLY set, the caller gives the
# complete module path (the CI jobs give a link farm of the native modules only), and none are added; qore reads it
# as set even when it is empty.
if [ -z "${QORE_MODULE_DIR_ONLY+set}" ] && [ -f "$QORE_BUILD_DIR/qore-external-module-dirs.txt" ]; then
    _external_module_dirs=`cat "$QORE_BUILD_DIR/qore-external-module-dirs.txt"`
    _old_ifs=$IFS
    IFS=:
    for _external_module_dir in $_external_module_dirs; do
        case ":$QORE_MODULE_DIR:" in
            *":$_external_module_dir:"*) ;;
            *)
                case "$QORE_MODULE_DIR" in
                    ""|*:) export QORE_MODULE_DIR=$QORE_MODULE_DIR$_external_module_dir ;;
                    *) export QORE_MODULE_DIR=$QORE_MODULE_DIR:$_external_module_dir ;;
                esac
                ;;
        esac
    done
    IFS=$_old_ifs
    unset _external_module_dirs _external_module_dir _old_ifs
fi
# Export build qore binary path so that tests spawning sub-processes (via backquote, etc.)
# use the same build binary rather than the system-installed qore.
export QORE_EXECUTABLE="$QORE"

# Keep qore tests hermetic by default. Developer shells often have Qorus or
# application provider discovery configured, which can make negative provider
# lookup tests print unrelated module-load diagnostics.
if [ "${QORE_TEST_PRESERVE_PROVIDER_ENV}" != "1" ]; then
    unset QORE_DATA_PROVIDERS
    unset QORE_PROVIDER_INDEX_DIR
fi

if [ $MEASURE_TIME -eq 1 ]; then
    # Test time commands.
    TIME_OK=0
    TIME_BIN=""
    TIME_CMD=""
    TIME_FORMAT="-------------------------------------\nUserTime: %U\nSystemTime: %S\nWallClockTime: %e\nMinorPageFaults: %R\nMajorPageFaults: %F\nAverageSharedTextSize: %X\nAverageUnsharedDataSize: %D\nAverageStackSize: %p\nAverageTotalSize: %K\nMaximumResidentSetSize: %M\nAverageResidentSetSize: %t\nFilesystemInputs: %I\nFilesystemOutputs: %O\nSocketMessagesSent: %s\nSocketMessagesReceived: %r\nExitStatus: %x"

    test_time() {
        TIME_CMD="$TIME_BIN -f \"$TIME_FORMAT\""
        eval $TIME_CMD ls / >/dev/null 2>&1
        TIME_OK=$?
    }

    TIME_BINS="time /usr/bin/time /bin/time `which time`"
    for tm in $TIME_BINS; do
        TIME_BIN=$tm
        test_time
        if [ $TIME_OK -eq 0 ]; then
            break
        fi
        TIME_CMD=""
    done

    if [ "$TIME_CMD" = "" ]; then
        TIME_CMD="time -p"
    fi
fi

# Put the libqore directory at the front of LD_LIBRARY_PATH only for build-tree
# binaries so they pick up the matching build libqore.so before any installed
# copy.  For installed/system binaries, do not prepend system library dirs:
# doing so can override the loader's normal dependency order for libraries such
# as ngtcp2/nghttp3/OpenSSL and mix incompatible stacks.
LIBQORE_DIR=$(dirname "$LIBQORE")
if [ "$QORE_IS_BUILD_TREE" = "1" ]; then
    export LD_LIBRARY_PATH="${LIBQORE_DIR}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
else
    case "$LIBQORE_DIR" in
        /lib|/lib/*|/usr/lib|/usr/lib/*) ;;
        *)
            case ":$LD_LIBRARY_PATH:" in
                *":$LIBQORE_DIR:"*) ;;
                *) export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}${LD_LIBRARY_PATH:+:}${LIBQORE_DIR}" ;;
            esac
            ;;
    esac
fi

# the build must match the tree, before anything runs: with the library path that the tests get (see above)
check_build

# Enable core dumps for crash diagnostics
ulimit -c unlimited 2>/dev/null
CORE_ULIMIT=$(ulimit -c 2>/dev/null)
echo "Core dump ulimit: ${CORE_ULIMIT:-unknown}"
# Create a directory for core dumps (used as CI artifact)
# Use a path relative to the project root so GitLab can collect it as an artifact
CORE_DIR="${CORE_DIR:-./crash-dumps}"
mkdir -p "$CORE_DIR" 2>/dev/null
CORE_DIR_ABS=$(cd "$CORE_DIR" && pwd)
# Try to set core pattern to write cores to our directory (may fail in containers).  Not in a CI job: Linux has one
# core pattern for the whole machine, which a CI job shares with the other jobs on its node, and a path in this tree
# does not exist in their containers, so their cores would be lost; the CI scripts set the pattern every job on the
# node sets (examples/test/docker_test/ci-core-capture.sh), and the cores are found below where it puts them.
CORE_PATTERN_SET=0
# The write itself tells whether the pattern was set: "-w" is true for root also where /proc/sys is read-only (a
# container), and the write then fails
if [ -z "$CI" ] && [ -w /proc/sys/kernel/core_pattern ] \
    && { echo "$CORE_DIR_ABS/core.%e.%p" > /proc/sys/kernel/core_pattern; } 2>/dev/null; then
    CORE_PATTERN_SET=1
fi
# Also try sysctl (works on some systems where /proc write fails)
if [ -z "$CI" ] && [ $CORE_PATTERN_SET -eq 0 ]; then
    sysctl -w "kernel.core_pattern=$CORE_DIR_ABS/core.%e.%p" 2>/dev/null && CORE_PATTERN_SET=1
fi
# macOS writes cores to kern.corefile (/cores/core.%P), which only root can write, so a crashing test left no
# core and no backtrace.  Only on disposable CI machines, where the system-wide setting does not outlive the job.
if [ $CORE_PATTERN_SET -eq 0 ] && [ "$(uname -s)" = "Darwin" ] && [ -n "$CI" ] && sudo -n true 2>/dev/null; then
    sudo -n sysctl -w "kern.corefile=$CORE_DIR_ABS/core.%P" > /dev/null 2>&1 && CORE_PATTERN_SET=1
    echo "kern.corefile: $(sysctl -n kern.corefile 2>/dev/null)"
fi
# Show the actual core pattern so we know where cores will land
if [ -f /proc/sys/kernel/core_pattern ]; then
    KERN_CORE_PATTERN=$(cat /proc/sys/kernel/core_pattern 2>/dev/null)
    echo "kernel.core_pattern: $KERN_CORE_PATTERN"
    if echo "$KERN_CORE_PATTERN" | grep -q '^|'; then
        echo "WARNING: core_pattern pipes to a program; cores may not be saved to disk"
    fi
fi

# OBS keeps core files under an absolute kernel pattern such as /.build/cores/%p.
# Derive a find(1) name pattern without eval, and only inspect files newer than
# the current test. Never mistake an earlier test's core for the current failure.
SYSTEM_CORE_DIR=""
SYSTEM_CORE_NAME=""
case "$KERN_CORE_PATTERN" in
    /*)
        SYSTEM_CORE_DIR=$(dirname "$KERN_CORE_PATTERN")
        case "$SYSTEM_CORE_DIR" in
            *%*) SYSTEM_CORE_DIR="" ;; # Dynamic directory components cannot be resolved here.
            *)
                SYSTEM_CORE_NAME=$(basename "$KERN_CORE_PATTERN" | awk '
                    {
                        for (i = 1; i <= length($0); ++i) {
                            c = substr($0, i, 1)
                            if (c == "%" && i < length($0)) {
                                n = substr($0, i + 1, 1)
                                if (n == "%") { printf "%%"; ++i; continue }
                                if (n ~ /[[:alpha:]]/) { printf "*"; ++i; continue }
                            }
                            if (c == "*" || c == "?" || c == "[" || c == "]" || c == "\\") {
                                printf "\\"
                            }
                            printf "%s", c
                        }
                    }')
                ;;
        esac
        ;;
esac

# the directories searched for the core of a test, in the order they are searched
CORE_SEARCH_DIRS='"$CORE_DIR_ABS" "$SYSTEM_CORE_DIR" . /tmp /var/lib/apport/coredump /var/crash'

# prints the find(1) name pattern of the core files in the given search directory
core_search_name() {
    if [ "$1" = "$SYSTEM_CORE_DIR" ]; then
        printf '%s\n' "$SYSTEM_CORE_NAME"
    elif [ "$1" = /var/crash ]; then
        printf '%s\n' '*.crash'
    else
        printf '%s\n' 'core*'
    fi
}

# prints the core files in the search directories, in the order they are searched
list_test_cores() {
    eval "set -- $CORE_SEARCH_DIRS"
    for CORE_SEARCH_DIR in "$@"; do
        if [ -n "$CORE_SEARCH_DIR" ] && [ -d "$CORE_SEARCH_DIR" ]; then
            find "$CORE_SEARCH_DIR" -maxdepth 1 -type f -name "$(core_search_name "$CORE_SEARCH_DIR")" -print \
                2>/dev/null
        fi
    done
}

# Prints the cores in the given search directory (the arguments: the directory and the find(1) name pattern of its
# cores) that the current test may have left: the files written after the test started, then those that did not exist
# before the test (TEST_CORES_BEFORE), as BusyBox find (Alpine) compares whole seconds and so does not find a core
# written in the second the test started
list_new_test_cores() {
    {
        find "$1" -maxdepth 1 -type f -name "$2" -newer "$TEST_START_MARKER" -print 2>/dev/null
        find "$1" -maxdepth 1 -type f -name "$2" -print 2>/dev/null | awk -v before="$TEST_CORES_BEFORE" \
            'BEGIN { while ((getline f < before) > 0) seen[f] = 1 } !($0 in seen)'
    } | awk '!($0 in listed) { listed[$0] = 1; print }'
}

# Returns success if the given core is of a process of the current test: the test, and every process it starts and
# those start, have QORE_TEST_PROCESS_MARK=<the test's mark> in their environment (see run_test()), which a core holds
# as a string of its own (an environment entry ends with a NUL byte)
core_has_test_mark() {
    LC_ALL=C tr -c 'A-Za-z0-9_=.' '\n' < "$1" 2>/dev/null | grep -qxF "QORE_TEST_PROCESS_MARK=$TEST_PROCESS_MARK"
}

# Sets CORE_FILE to the core the current test left (see list_new_test_cores())
find_test_core() {
    CORE_FILE=""
    eval "set -- $CORE_SEARCH_DIRS"
    for CORE_SEARCH_DIR in "$@"; do
        if [ -z "$CORE_SEARCH_DIR" ] || [ ! -d "$CORE_SEARCH_DIR" ]; then
            continue
        fi
        NEW_CORES=$(list_new_test_cores "$CORE_SEARCH_DIR" "$(core_search_name "$CORE_SEARCH_DIR")")
        if [ -z "$NEW_CORES" ]; then
            continue
        fi
        if [ "$TEST_JOBS" -le 1 ]; then
            CORE_FILE=$(printf '%s\n' "$NEW_CORES" | head -1)
            break
        fi
        # Tests that run at the same time (TEST_JOBS > 1) write their cores to the same places, so a new core is the
        # test's only when its name has the process ID of the test as a part of its own (core.%e.%p.%t on Linux CI,
        # core.%P on macOS CI), or when it holds the test's mark: the core of a process that the test started
        if [ -n "$TEST_PID" ]; then
            CORE_FILE=$(printf '%s\n' "$NEW_CORES" | awk -v pid="$TEST_PID" \
                '{ n = split($0, path, "/"); m = split(path[n], part, ".") }
                { for (j = 2; j <= m; ++j) { if (part[j] == pid) { print; exit } } }')
        fi
        if [ -z "$CORE_FILE" ]; then
            while IFS= read -r core; do
                if core_has_test_mark "$core"; then
                    CORE_FILE=$core
                    break
                fi
            done <<EOF
$NEW_CORES
EOF
        fi
        if [ -n "$CORE_FILE" ]; then
            break
        fi
    done
}

# Prints the name of the given test in the names of its files in CORE_DIR: its path in the test tree with '_' for '/',
# as tests in different directories can have the same file name
test_artifact_name() {
    _name=${1#./}
    _name=${_name#examples/test/}
    _name=${_name%.qtest}
    printf '%s\n' "$_name" | tr '/' '_'
}

# The pipes the output of the tests goes to (Linux only): a CI job reads its output through a pipe and ends only when
# no process holds it any more.  run_tests.sh and the processes that started it hold them as they should.
TEST_OUTPUT_PIPES=""
RUN_TESTS_ANCESTORS=""
if [ -r /proc/self/environ ]; then
    for fd in 1 2; do
        target=`readlink /proc/$$/fd/$fd 2>/dev/null`
        case "$target" in
            pipe:*) TEST_OUTPUT_PIPES="$TEST_OUTPUT_PIPES $target" ;;
        esac
    done
    RUN_TESTS_ANCESTORS=$$
    ancestor=$$
    while [ "$ancestor" -gt 1 ]; do
        ancestor=`awk '/^PPid:/ { print $2 }' /proc/$ancestor/status 2>/dev/null`
        if [ -z "$ancestor" ]; then
            break
        fi
        RUN_TESTS_ANCESTORS="$RUN_TESTS_ANCESTORS $ancestor"
    done
fi

# qore_ci_running_processes(): tells the processes a test left running from a BusyBox timeout watcher and from one
# that has ended meanwhile
. "$BASE_TEST_PATH/docker_test/ci-job-processes.sh"

# Prints the processes that descend from the given process (this runner by default; Linux only): its children,
# theirs, and so on.  Each status file is read with getline, which skips one that is gone (a process that ended
# during the scan): awk given them as input files would end at a missing one, and print nothing.  /proc is
# QORE_TEST_PROC if set, for the tests of run_tests.sh.
runner_descendants() {
    awk -v root=${1:-$$} -v proc="${QORE_TEST_PROC:-/proc}" 'BEGIN {
        for (i = 1; i < ARGC; ++i) {
            f = ARGV[i]
            pid = substr(f, length(proc) + 2)
            sub(/\/status$/, "", pid)
            while ((getline line < f) > 0) {
                if (line ~ /^PPid:/) {
                    split(line, field)
                    parent[pid] = field[2]
                    break
                }
            }
            close(f)
        }
        found[root] = 1
        # each pass adds the children of the processes found so far, until a pass adds none
        do {
            added = 0
            for (p in parent) {
                if (!(p in found) && (parent[p] in found)) {
                    found[p] = 1
                    added = 1
                }
            }
        } while (added)
        for (p in found) {
            if (p != root) {
                printf "%s ", p
            }
        }
        exit
    }' "${QORE_TEST_PROC:-/proc}"/[0-9]*/status 2>/dev/null
}

# Sets TEST_SURVIVORS to the processes the current test started that are still running (Linux only).  Every process a
# test starts, and every process those start, inherits QORE_TEST_PROCESS_MARK in its environment, also when it
# leaves the test's session or is adopted by another parent, so a process that still has it when the test has ended
# was not ended by the test.  Any other process that descends from this runner and holds the output of the tests as
# its standard output or error, such as one a test started with an environment of its own, is one too: only one that
# descends from it, as other runners (the shards of one job) can share the output and are no survivors of this one's
# tests.  The trade-off, chosen to protect sibling shards: a process without the mark that has left this runner's
# process tree (adopted by init after its parent ended) holding the output is not found.  The output of a test of a
# parallel run is its log, in the run's own directory, which no other runner holds: every process that holds it is
# one, wherever it is in the process tree.
# SURVIVORS_NOTED: the "mark/pid" of each process that find_test_survivors has noted as excused
SURVIVORS_NOTED=""
find_test_survivors() {
    TEST_SURVIVORS=""
    if [ ! -r /proc/self/environ ]; then
        return
    fi
    # the output of a test of a parallel run is its log (TEST_LOG, an absolute path without spaces)
    test_outputs="$TEST_OUTPUT_PIPES"
    outputs_shared=1
    if [ -n "$TEST_LOG" ]; then
        test_outputs="$TEST_LOG"
        outputs_shared=0
    fi
    if [ -n "$test_outputs" ]; then
        # the commands that look for them must not hold the output themselves
        TEST_SURVIVORS=`{ ls -l /proc/[0-9]*/fd/1 /proc/[0-9]*/fd/2 | awk -v pipes="$test_outputs" \
            -v own="$RUN_TESTS_ANCESTORS" -v shared=$outputs_shared -v descendants="$(runner_descendants)" 'BEGIN {
                n = split(pipes, list, " ")
                for (i = 1; i <= n; ++i) {
                    pipe[list[i]] = 1
                }
                n = split(own, list, " ")
                for (i = 1; i <= n; ++i) {
                    skip[list[i]] = 1
                }
                n = split(descendants, list, " ")
                for (i = 1; i <= n; ++i) {
                    mine[list[i]] = 1
                }
            }
            $(NF - 1) == "->" && ($NF in pipe) {
                split($(NF - 2), path, "/")
                if ((!shared || (path[3] in mine)) && !(path[3] in skip) && !(path[3] in seen)) {
                    seen[path[3]] = 1
                    printf "%s ", path[3]
                }
            }'; } 2>/dev/null`
    fi
    # the processes that hold the test's output (found above), which no namespace excuses
    survivor_output_holders=" $TEST_SURVIVORS "
    TEST_SURVIVORS="$TEST_SURVIVORS"`awk -v mark="QORE_TEST_PROCESS_MARK=$1" 'BEGIN {
        RS = "\0"
        for (i = 1; i < ARGC; ++i) {
            f = ARGV[i]
            while ((getline e < f) > 0) {
                if (e == mark) {
                    sub(/^\/proc\//, "", f)
                    sub(/\/environ$/, "", f)
                    printf "%s ", f
                    break
                }
            }
            close(f)
        }
        exit
    }' /proc/[0-9]*/environ`
    # a process found both ways is listed once
    if [ -n "$TEST_SURVIVORS" ]; then
        TEST_SURVIVORS=`printf '%s\n' $TEST_SURVIVORS | sort -un | tr '\n' ' '`
    fi
    # A process in a user namespace of its own that has the test's mark but does not hold the test's output is not
    # reported: it is most likely a per-user helper that a container runtime made as a side effect of a command the
    # test ran, which outlives the command and is shared by every later command of the user.  Rootless podman does
    # this: its first command starts a pause process (catatonit -P) that holds the user's namespaces for all of the
    # user's containers, and ending it breaks every one of them, also those of other jobs on the machine.  It is
    # found by the namespace (a test's own processes share the runner's user namespace), not by its name.  What stays
    # undetected: a leak inside a child user namespace that holds none of the test's output (it cannot keep a CI job
    # from ending).  A process that holds the test's output is always a survivor, whatever its namespace, and so is
    # one whose namespace cannot be read.
    if [ -n "$TEST_SURVIVORS" ] && survivor_runner_ns=`readlink /proc/$$/ns/user 2>/dev/null` \
        && [ -n "$survivor_runner_ns" ]; then
        survivors_reported=""
        for survivor_pid in $TEST_SURVIVORS; do
            survivor_ns=`readlink /proc/$survivor_pid/ns/user 2>/dev/null`
            case "$survivor_output_holders" in
                *" $survivor_pid "*) survivor_ns="" ;;
            esac
            if [ -z "$survivor_ns" ] || [ "$survivor_ns" = "$survivor_runner_ns" ]; then
                survivors_reported="$survivors_reported$survivor_pid "
            else
                # find_test_survivors runs more than once for a test (after it, and when a run is interrupted): the
                # note is printed once per process of a test
                case "$SURVIVORS_NOTED" in
                    *" $1/$survivor_pid "*) ;;
                    *)
                        SURVIVORS_NOTED="$SURVIVORS_NOTED $1/$survivor_pid "
                        survivor_cmd=`tr '\0' ' ' < /proc/$survivor_pid/cmdline 2>/dev/null`
                        echo "note: not ending process $survivor_pid: $survivor_cmd: it is in a user namespace of" \
                            "its own and holds no output of the test" >&2
                        ;;
                esac
            fi
        done
        TEST_SURVIVORS=$survivors_reported
    fi
}

# Print info about used variables etc.
echo "Using qore: $QORE"
echo "Using libqore: $LIBQORE"
echo "QORE_INCLUDE_DIR=$QORE_INCLUDE_DIR"
echo "QORE_MODULE_DIR=$QORE_MODULE_DIR"
echo "LD_PRELOAD=$LD_PRELOAD"
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
# Prints a datasource string with its password replaced by "****", for logs that others can read (CI job logs)
# The string is split as parseDatasource() splits it: an option string ("{...}") at the end is kept as is, the
# driver name ends at the first ':', the user name at the first '/' and the password at the last '@'
mask_connstr() {
    _cs=$1
    _opts=""
    case "$_cs" in
        *\{*\})
            _opts="{${_cs##*\{}"
            _cs=${_cs%\{*}
            ;;
    esac
    printf '%s%s\n' "$(printf '%s\n' "$_cs" | sed -e 's|^\(\([^:/@%]*:\)\{0,1\}[^/]*\)/.*@|\1/****@|')" "$_opts"
}
echo "QORE_DB_CONNSTR: $(mask_connstr "${QORE_DB_CONNSTR}")"
echo "QORE_DB_CONNSTR_FREETDS: $(mask_connstr "${QORE_DB_CONNSTR_FREETDS}")"
echo "QORE_DB_CONNSTR_MYSQL: $(mask_connstr "${QORE_DB_CONNSTR_MYSQL}")"
echo "QORE_DB_CONNSTR_PGSQL: $(mask_connstr "${QORE_DB_CONNSTR_PGSQL}")"
echo "QORE_DB_CONNSTR_ORACLE: $(mask_connstr "${QORE_DB_CONNSTR_ORACLE}")"
# Mask credentials in REDIS_URL (replace user:pass@ with ***@)
echo "REDIS_URL: $(echo "${REDIS_URL}" | sed 's|://[^@]*@|://***@|')"

if [ $MEASURE_TIME -eq 1 ]; then
    printf "TIME_CMD: %s\n" "$TIME_CMD"
fi
echo

# a run without tests ends here, once it has printed its settings, which run-tests-credentials.qtest reads (-l ends
# above, before the settings are known)
if [ $TEST_COUNT -eq 0 ]; then
    echo "ERROR: no tests found to run" >&2
    exit 1
fi

PASSED_TEST_COUNT=0
FAILED_TEST_COUNT=0
# the tests whose watchdog failed (see run_with_watchdog()), and how many of them passed
WATCHDOG_ERROR_TESTS=""
WATCHDOG_ERROR_COUNT=0
WATCHDOG_ERROR_PASSED_COUNT=0

# The number of tests run at the same time: QORE_TEST_JOBS, a number or "auto" (the number of CPUs); 1 by default.
# A parallel run prints the output of each test when the test has ended, as a whole.  A test that cannot run beside
# others declares a lane on its own line:
#   # test-parallel: serial - <why>     one serial test at a time, beside the other tests (ex: tests that create
#                                       objects with fixed names in a database schema that has no per-run schema);
#                                       <why> names the shared resource: tests that only share names are a bug
#   # test-parallel: exclusive - <why>  no other test at the same time (ex: tests of the limits of the machine, or of
#                                       timing that the load of other tests changes)
# examples/test/runner/test_parallel_runner.py checks the parallel runner.
case "${QORE_TEST_JOBS:-1}" in
    auto)
        TEST_JOBS=`getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1`
        ;;
    *[!0-9]*|"")
        echo "ERROR: QORE_TEST_JOBS is a number of tests or \"auto\": '$QORE_TEST_JOBS'" >&2
        exit 1
        ;;
    *)
        TEST_JOBS=${QORE_TEST_JOBS:-1}
        ;;
esac
if [ "$TEST_JOBS" -lt 1 ] 2>/dev/null; then
    TEST_JOBS=1
fi
if [ $MEASURE_TIME -eq 1 ] && [ "$TEST_JOBS" -gt 1 ]; then
    echo "-t measures one test at a time: QORE_TEST_JOBS=$QORE_TEST_JOBS is ignored"
    TEST_JOBS=1
fi
if [ "$TEST_JOBS" -gt "$TEST_COUNT" ]; then
    TEST_JOBS=$TEST_COUNT
fi

# Run single test with timeout (default 300s = 5 minutes).
# A test that legitimately needs more time than the default declares its own budget with a
# "# test-timeout: <seconds>" comment on its own line; the default still guards every other test, so a
# test that hangs is still reported quickly.
# Use gtimeout on macOS (GNU coreutils), timeout on Linux.
TEST_TIMEOUT=${TEST_TIMEOUT:-300}
# (TIMEOUT_CMD is set above, before the build is checked)

# GNU timeout aborts a test that exceeds its time limit, so the test dumps core and the stacks of its threads
# show where it hangs; BusyBox timeout (Alpine) keeps SIGTERM, as the exit code of an abort there would be
# taken for a crash
TIMEOUT_SIGNAL_OPTS=""
if [ -n "$TIMEOUT_CMD" ] && $TIMEOUT_CMD --version 2>/dev/null | grep -q GNU; then
    TIMEOUT_SIGNAL_OPTS="-s ABRT -k 60"
fi

# The files of this runner, removed by their exact names when it ends
TEST_START_MARKER=""
TEST_CORES_BEFORE=""
TEST_EVENT_DIR=""
TEST_EVENT_FIFO=""
remove_runner_files() {
    for f in "$TEST_START_MARKER" "$TEST_CORES_BEFORE" "$TEST_EVENT_FIFO"; do
        if [ -n "$f" ]; then
            rm -f "$f"
        fi
    done
    if [ -n "$TEST_EVENT_DIR" ]; then
        rmdir "$TEST_EVENT_DIR" 2>/dev/null
    fi
}
trap remove_runner_files 0
# The test that runs and its watchdog (see run_with_watchdog()), while one runs; *_starting is 1 from just before the
# process is started until its ID is recorded, so that the trap below takes it from $! in that window if $! has
# changed since the flag was set (*_before_pid, $! then: the previous test's watchdog, for example)
test_pid=""
test_starting=0
# the process group of the test (its process ID; see exec_in_test_group()) from its start until it has been ended or
# left to the checks after the test
test_group=""
test_before_pid=""
watchdog_pid=""
watchdog_starting=0
watchdog_before_pid=""
# Returns 0 if the given process has the given QORE_TEST_PROCESS_MARK in its environment (/proc on Linux, ps -E
# elsewhere): it is then the test that got the mark, or a process it started, never a process that reused the test's
# process ID after the test had ended (a zombie has no environment)
test_has_mark() {
    if [ -r /proc/self/environ ]; then
        tr '\0' '\n' < /proc/$1/environ 2>/dev/null | grep -qx "QORE_TEST_PROCESS_MARK=$2"
    else
        # ps -E prints the environment after the arguments: they are removed first, so that a process whose arguments
        # name the mark (the "env QORE_TEST_PROCESS_MARK=..." that starts a test) does not count.  If the process
        # executes another program between the two calls, its arguments are not removed, and are searched too.
        mark_args=`ps -ww -o command= -p $1 2>/dev/null`
        mark_all=`ps -E -ww -o command= -p $1 2>/dev/null`
        case " ${mark_all#"$mark_args"} " in
            *" QORE_TEST_PROCESS_MARK=$2 "*) return 0 ;;
        esac
        return 1
    fi
}

# A test that the watchdog watches (see run_with_watchdog()) runs as the leader of a process group of its own, so
# that every process it starts, and every process those start, can be ended with it when it exceeds its time limit or
# the run is interrupted, on every platform: a process that a test leaves is otherwise found only on Linux (see
# find_test_survivors()), and nothing ended the processes of a timed-out test elsewhere (qore#6835: macOS kept the
# children of such tests for days).  The group is made by perl (setpgid(), in the runner's session), or by setsid
# (a session of its own, which is a process group too) where there is no perl; there is no portable shell command for
# it ("set -m" needs a terminal in dash and BusyBox sh).  Without either, or if perl cannot make the group, the test is
# signalled alone, as before.  A process that a test starts and that leaves the group (setsid(), setpgid()) is not
# ended with it.
TEST_GROUP_TOOL=""
if command -v perl > /dev/null 2>&1 && perl -e 'exit 0' > /dev/null 2>&1; then
    TEST_GROUP_TOOL=perl
elif command -v setsid > /dev/null 2>&1; then
    TEST_GROUP_TOOL=setsid
fi

# Replaces the shell with the given command, as the leader of a process group of its own if it can (see above).  The
# shell is a process run in the background by a non-interactive shell, so it is not the leader of a group already:
# setsid would otherwise fork, and its process would not be the test's.
exec_in_test_group() {
    case "$TEST_GROUP_TOOL" in
        perl)
            # a group that cannot be made is no reason not to run the test: it runs in the runner's group then
            exec perl -e 'setpgrp(0, 0) or warn "run_tests.sh: the test runs without a process group of its own:" .
                    " setpgrp: $!\n";
                exec { $ARGV[0] } @ARGV or die "exec $ARGV[0]: $!\n"' -- "$@"
            ;;
        setsid) exec setsid "$@" ;;
    esac
    exec "$@"
}

# The shell that starts the tests (this runner, or the shell that runs a test of a parallel run; see
# run_tests_parallel()): the parent of each test's process
RUNNER_SHELL_PID=$$

# Returns 0 if the given process is a child of RUNNER_SHELL_PID that the shell has not waited for (it runs, or is a
# zombie): its ID, and the ID of the group it leads, then belong to the test.  Not the test's QORE_TEST_PROCESS_MARK
# (test_has_mark()): macOS shows no environment of a system binary (ps -E), such as the perl or env that starts the
# test, or a test that is a shell script.  /proc on Linux, read by a builtin, so that the shell waits for no child
# between the check and the signal; ps elsewhere, while which the shell may wait for the test if it ends just then:
# its ID would then have to be taken by another process in the instant until the signal.
test_is_shell_child() {
    child_parent=""
    if [ -r /proc/self/stat ]; then
        child_stat=""
        { IFS= read -r child_stat < /proc/$1/stat; } 2>/dev/null
        # the fields after the command name, which is in parentheses and may contain either: the state, the parent
        child_stat=${child_stat##*) }
        child_parent=${child_stat#* }
        child_parent=${child_parent%% *}
    else
        child_parent=`ps -o ppid= -p $1 2>/dev/null`
    fi
    [ -n "$child_parent" ] && [ $child_parent -eq $RUNNER_SHELL_PID ] 2>/dev/null
}

# Sends the given signal to the process group of the test whose process is given (it leads the group), or to the
# test's process alone if there is no such group (it has not made it yet, or cannot).  Nothing is sent if the process
# is no longer the shell's child: a shell waits for a background child that has ended during any command it runs in
# the foreground (sh, dash), and keeps its status for a "wait" later, so the test may have ended and been waited for
# already, and its ID may be another process's; the group, if anything is left of it, is killed once the test has been
# waited for (end_test_group()).  Whether the group exists is checked before the signal, rather than taken from its
# result, which is a failure also when it reaches some of the group but not all (another user's process): the test's
# process would get the signal twice.
signal_test_group() {
    if ! test_is_shell_child $2; then
        return
    fi
    if [ -n "$TEST_GROUP_TOOL" ] && kill -0 -$2 2>/dev/null; then
        kill -$1 -$2 2>/dev/null
    else
        kill -$1 $2 2>/dev/null
    fi
}

# Kills what is left of the process group of the test whose process is given, once that process has ended and has been
# waited for (see exec_in_test_group()).  The group's ID cannot be taken by another process or group while the group
# has a process; once it has none, the signal finds no group, unless every process ID has been used since the test was
# waited for, an instant before, and the process that got the ID leads a group.  The test's QORE_TEST_PROCESS_MARK
# cannot tell: macOS shows no environment of a system binary (ps -E), such as a cat that the test started.
end_test_group() {
    if [ -n "$TEST_GROUP_TOOL" ] && [ -n "$1" ]; then
        kill -KILL -$1 2>/dev/null
    fi
}

# Ends the processes the current test left (QORE_TEST_PROCESS_MARK, Linux only), except the given test process
end_test_survivors() {
    if [ -z "$TEST_PROCESS_MARK" ]; then
        return
    fi
    find_test_survivors "$TEST_PROCESS_MARK"
    if [ -z "$TEST_SURVIVORS" ]; then
        return
    fi
    runner_survivors=""
    while read -r kind pid cmd; do
        case "$kind" in
            watcher|running)
                if [ "$pid" != "$1" ]; then
                    runner_survivors="$runner_survivors $pid"
                fi
                ;;
        esac
    done <<EOF
`qore_ci_running_processes $TEST_SURVIVORS`
EOF
    if [ -n "$runner_survivors" ]; then
        kill -KILL $runner_survivors 2>/dev/null
    fi
}
# Ends the runner for a signal (the name and the number): a shell does not run its EXIT trap when a signal ends it, so
# this does what the end of a run does, in this order:
# - the watchdog of the test that runs is stopped ("exit" on its FIFO, which ends it in every phase without ending
#   the test, and ends its FIFO reader, which GNU timeout runs in a process group of its own, where a SIGHUP from a
#   terminal does not reach it): left running, it would end a process minutes later that may have reused the test's
#   process ID;
# - the processes the test left are ended at once (a CI executor may end the job soon after the signal);
# - the test is sent the signal (SIGTERM for SIGINT, which a command that a non-interactive shell runs in the
#   background ignores, so that a SIGINT from a terminal does not reach it either) and waited for, for at most
#   TEST_INTERRUPT_WAIT seconds: a helper waits that long for the trap to report that the test has ended, and sends
#   it SIGKILL if it has not (a test that handles the signal and hangs);
# - what the test left meanwhile is ended, and the files are removed;
# - the runner ends by the signal itself, so that its caller (a loop in bash, for example) sees why.
# A test that runs in the foreground (without the watchdog) is waited for before a trap runs, as a shell defers traps
# until its foreground command has ended.
# The shell that runs a test of a parallel run (a third argument, "job"; see run_tests_parallel()) ends its test in
# the same way, and then exits with 128 plus the number of the signal: its files are the runner's, which removes them,
# and $$ there is the runner, which is not signalled again.
runner_signaled() {
    # a second signal while the trap runs is ignored: re-entered, the trap would start a second helper and lose the
    # first one, which would hold the output and act on a test long gone; re-raising the signal below resets it
    trap '' HUP INT TERM
    # the words are split as usual: a trap that interrupts "IFS= read" runs with that IFS in bash
    unset IFS
    if [ -z "$test_pid" ] && [ $test_starting -eq 1 ] && [ "$!" != "$test_before_pid" ]; then
        test_pid=$!
    fi
    if [ -z "$watchdog_pid" ] && [ $watchdog_starting -eq 1 ] && [ "$!" != "$watchdog_before_pid" ]; then
        watchdog_pid=$!
    fi
    if [ -n "$watchdog_pid" ]; then
        echo exit >&4 2>/dev/null
        wait $watchdog_pid 2>/dev/null
        watchdog_pid=""
    fi
    end_test_survivors "$test_pid"
    # the group of a test that has ended and been waited for, whose processes were not ended yet
    if [ -z "$test_pid" ] && [ -n "$test_group" ]; then
        end_test_group $test_group
        test_group=""
    fi
    if [ -n "$test_pid" ]; then
        # the signal goes to every process of the test (see exec_in_test_group())
        if [ "$1" = INT ]; then
            signal_test_group TERM $test_pid
        else
            signal_test_group $1 $test_pid
        fi
        # the helper reads the trap's report that the test has ended from a FIFO of the runner's own, for at most
        # TEST_INTERRUPT_WAIT seconds, and then kills the test, but only a process that still has the test's
        # QORE_TEST_PROCESS_MARK (see test_has_mark()): if the test ended and was waited for just before, its process
        # ID may belong to another process by then, which has not.  The FIFO is open for reading and writing here, so
        # that neither side blocks.  The helper kills the test itself rather than signalling the runner to do it: a
        # trapped signal runs another trap inside this one while it waits (dash runs a second TERM's trap there), so
        # the runner's kill would have to be safe in a nested trap; the mark check makes the helper's kill safe
        # instead.  If the helper cannot be started, the test is killed at once (once its mark is confirmed), so the
        # wait below is always bounded.
        interrupt_fifo="$TEST_EVENT_DIR/interrupt"
        interrupt_helper=""
        if [ -n "$TIMEOUT_CMD" ] && mkfifo "$interrupt_fifo" 2>/dev/null && (exec 5<>"$interrupt_fifo") 2>/dev/null; then
            exec 5<>"$interrupt_fifo"
            (
                # the signals the trap ignores would be ignored by the commands run here too (an ignored signal is
                # inherited): the timeout command could not end its reader with SIGTERM
                trap - HUP INT TERM
                # this process is not the shell that waits for the test: the shell may wait for it between the check
                # in signal_test_group() and the signal (the ID would then have to be taken in that instant)
                if ! $TIMEOUT_CMD $TEST_INTERRUPT_WAIT sh -c 'read -r ended' <&5 2>/dev/null \
                        && test_has_mark $test_pid "$TEST_PROCESS_MARK"; then
                    echo "*** the run is interrupted, and the test did not end within ${TEST_INTERRUPT_WAIT}s:" \
                        "SIGKILL sent ***"
                    signal_test_group KILL $test_pid
                fi
            ) &
            interrupt_helper=$!
        elif test_has_mark $test_pid "$TEST_PROCESS_MARK"; then
            echo "*** the run is interrupted, and the runner cannot wait for the test for a limited time: SIGKILL" \
                "sent ***"
            signal_test_group KILL $test_pid
        fi
        # for the tests of run_tests.sh: reports that the trap waits for the test
        if [ -n "$QORE_TEST_RUNNER_TRAP_REPORT" ]; then
            echo waiting > "$QORE_TEST_RUNNER_TRAP_REPORT"
        fi
        wait $test_pid 2>/dev/null
        # what is left of the test's group: processes that handle or ignore the signal
        end_test_group $test_pid
        test_pid=""
        test_group=""
        if [ -n "$interrupt_helper" ]; then
            echo ended >&5
            wait $interrupt_helper 2>/dev/null
            exec 5>&-
        fi
        rm -f "$interrupt_fifo"
        end_test_survivors
    fi
    if [ "$3" = job ]; then
        exit $((128 + $2))
    fi
    remove_runner_files
    trap - 0 $1
    kill -$1 $$
    # if the signal did not end the shell
    exit $((128 + $2))
}
trap 'runner_signaled HUP 1' HUP
trap 'runner_signaled INT 2' INT
trap 'runner_signaled TERM 15' TERM
# A separate marker per runner also permits concurrent test shards.
TEST_START_MARKER=$(mktemp "$CORE_DIR_ABS/test-start.XXXXXX") || exit 1
# the core files there were when the current test started (see find_test_core())
TEST_CORES_BEFORE=$(mktemp "$CORE_DIR_ABS/test-cores.XXXXXX") || exit 1
# The FIFO through which a test and run_tests.sh report events to the watchdog of the test (see run_with_watchdog()),
# in a directory that only this runner uses: no other runner can open or remove it, whatever process IDs the runners
# have and whatever tree they share.  It was named after the runner's process ID in the core directory, so runners in
# containers that shared a tree, each of them process 2, used the same FIFO: another runner removed it between this
# runner's mkfifo and its open, which then made a regular file, and the watchdog took the end of that file for the
# time limit (a test was sent SIGUSR1 20 ms after it started and reported as having exceeded its 600 s limit).
TEST_EVENT_TMP=${TMPDIR:-/tmp}
TEST_EVENT_DIR=$(mktemp -d "${TEST_EVENT_TMP%/}/qore-test-events.XXXXXX") || exit 1
TEST_EVENT_FIFO="$TEST_EVENT_DIR/events"

# A test that exceeds its time limit is first sent SIGUSR1 when the signal cannot end it (see run_with_watchdog()):
# a QUnit test then prints the call stacks of all its threads and reports it through the event FIFO.  Once it has
# reported it, has ended, or QORE_TEST_STACK_DUMP_WAIT seconds (default 30) have passed, it is aborted (SIGABRT), so
# that it dumps core, and sent SIGKILL 60 seconds later.  A test that SIGUSR1 would end, without a core dump and
# without its call stacks, such as one that hangs before QUnit has installed its handler, or one run with signal
# handling disabled (-b), is aborted at once.  The watchdog aborts the test with any timeout command: its exit code is
# 124 when it ends the test, so the abort is not taken for a crash, as it is when BusyBox timeout ends a test (which
# keeps SIGTERM: see TIMEOUT_SIGNAL_OPTS).
TEST_STACK_DUMP_WAIT=${QORE_TEST_STACK_DUMP_WAIT:-30}
# the seconds a test may take to end after an interrupted run sent it a signal (see runner_signaled())
TEST_INTERRUPT_WAIT=${QORE_TEST_INTERRUPT_WAIT:-10}
TEST_TIMEOUT_SIGNAL=ABRT
TEST_SIGNALS_ENABLED=1
for opt in $QORE_TEST_OPTS; do
    case "$opt" in
        -b|--disable-signals) TEST_SIGNALS_ENABLED=0 ;;
    esac
done

# The events that the watchdog of a test reads from the event FIFO, one per line:
# - "handler <pid>": QUnit has installed its SIGUSR1 handler, which prints the call stacks of the test's threads, in
#   process <pid> (see qlib/QUnit.qm)
# - "dumped": that handler has printed them
# - "exit": the test has ended (run_with_watchdog())

# Prints the events read from the event FIFO (open on fd 4), one per line as "event <event>", until one of the events
# given after the number of seconds has been read or the time has passed; prints "eof" if the FIFO ends instead.
# Returns 0 when an event given or the end of the FIFO has been read, and the status of the timeout command when the
# time has passed: 124 (GNU and uutils timeout), or 143 when BusyBox timeout has ended the read with SIGTERM.  The
# reader has the FIFO open for writing as well, so a FIFO never ends: what ends is a file that is not the FIFO.
read_test_event() {
    read_secs=$1
    shift
    $TIMEOUT_CMD $read_secs sh -c '
        while IFS= read -r event; do
            printf "event %s\n" "$event"
            for end in "$@"; do
                if [ "$event" = "$end" ]; then
                    exit 0
                fi
            done
        done
        echo eof' sh "$@" <&4 2>/dev/null
}

# Prints a message of the watchdog where the output of the test goes (TEST_LOG in a parallel run)
watchdog_message() {
    if [ -n "$TEST_LOG" ]; then
        echo "$@" >> "$TEST_LOG"
    else
        echo "$@"
    fi
}

# Runs a test (the arguments after the time limit) with its time limit; returns the test's exit code, or 124 if it
# exceeded its time limit.  Sets WATCHDOG_TEST_EXIT to the test's own exit code, WATCHDOG_TIMED_OUT to 1 if the
# watchdog ended the test as it exceeded its time limit, and WATCHDOG_ERROR to 1 if the watchdog failed; both are 0
# otherwise.  A watchdog that fails never ends a test for it, and never reports it as having exceeded its time limit;
# the test's result is its own, and the run fails as a harness error (see WATCHDOG_ERROR_TESTS).
run_with_watchdog() {
    limit=$1
    shift
    WATCHDOG_TIMED_OUT=0
    WATCHDOG_ERROR=0
    rm -f "$TEST_EVENT_FIFO"
    # The FIFO is opened for reading and writing, so that neither the writes below nor the test's block; a redirection
    # of exec that fails ends the shell, so it is opened in a subshell first.  Descriptor 4 must be the FIFO: a read of
    # any other file ends at once without an event, which the watchdog took for the time limit when the FIFO had been
    # removed before it was opened, and the open had made a regular file.  /dev/fd/4 is this shell's descriptor where
    # /dev/fd exists (bash also has it without); elsewhere the name is checked, which no other runner uses.
    watchdog_fifo=0
    if ! mkfifo "$TEST_EVENT_FIFO" || ! (exec 4<>"$TEST_EVENT_FIFO") 2>/dev/null; then
        watchdog_fifo_error="cannot be made or opened"
    else
        exec 4<>"$TEST_EVENT_FIFO"
        if [ -e /dev/fd/4 ]; then
            if [ -p /dev/fd/4 ]; then
                watchdog_fifo=1
            fi
        elif [ -p "$TEST_EVENT_FIFO" ]; then
            watchdog_fifo=1
        fi
        if [ $watchdog_fifo -eq 0 ]; then
            exec 4>&-
            watchdog_fifo_error="was removed before it was opened, and the open made a file that is not a FIFO"
        fi
    fi
    # Without the FIFO, the time limit is kept by the timeout command
    if [ $watchdog_fifo -eq 0 ]; then
        rm -f "$TEST_EVENT_FIFO"
        watchdog_message "*** WATCHDOG ERROR: the watchdog's event FIFO $TEST_EVENT_FIFO $watchdog_fifo_error:" \
            "the test is run with a plain time limit ***"
        WATCHDOG_ERROR=1
        if [ -n "$TEST_LOG" ]; then
            $TIMEOUT_CMD $TIMEOUT_SIGNAL_OPTS $limit "$@" >> "$TEST_LOG" 2>&1
        else
            $TIMEOUT_CMD $TIMEOUT_SIGNAL_OPTS $limit "$@"
        fi
        WATCHDOG_TEST_EXIT=$?
        return $WATCHDOG_TEST_EXIT
    fi
    test_before_pid=$!
    test_starting=1
    # a parallel run writes the output of the test to its log (TEST_LOG).  The test runs in a process group of its
    # own (see exec_in_test_group()); the mark is in its environment from the start, so that the process has it also
    # before the command that makes the group has replaced itself with the test
    if [ -n "$TEST_LOG" ]; then
        (
            QORE_TEST_EVENT_FIFO="$TEST_EVENT_FIFO" QORE_TEST_PROCESS_MARK="$TEST_PROCESS_MARK"
            export QORE_TEST_EVENT_FIFO QORE_TEST_PROCESS_MARK
            exec_in_test_group "$@"
        ) 4>&- >> "$TEST_LOG" 2>&1 &
    else
        (
            QORE_TEST_EVENT_FIFO="$TEST_EVENT_FIFO" QORE_TEST_PROCESS_MARK="$TEST_PROCESS_MARK"
            export QORE_TEST_EVENT_FIFO QORE_TEST_PROCESS_MARK
            exec_in_test_group "$@"
        ) 4>&- &
    fi
    test_pid=$!
    test_group=$test_pid
    test_starting=0
    # the process of the test (env executes qore), whose core is found by its process ID (see find_test_core())
    TEST_PID=$test_pid
    # The watchdog reads the test's events until its time limit, and ends the test when the limit has passed.  Its exit
    # status has bit 0 set if the limit has passed, and bit 1 if its read of the FIFO failed.
    watchdog_before_pid=$!
    watchdog_starting=1
    (
        # the watchdog's messages go where the test's output goes
        if [ -n "$TEST_LOG" ]; then
            exec >> "$TEST_LOG" 2>&1
        fi
        status=0

        # Reads events into events (see read_test_event()); returns 0 if one of the events given after the number of
        # seconds has been read, 1 if the time has passed, and 2 if the read ended otherwise, which it cannot, as
        # descriptor 4 is the FIFO, which the reader also has open for writing
        watchdog_wait() {
            events=`read_test_event "$@"`
            case $? in
                0)
                    case "$events" in
                        eof|*"
eof") ;;
                        *) return 0 ;;
                    esac
                    ;;
                124|143) return 1 ;;
            esac
            if [ $((status & 2)) -eq 0 ]; then
                echo "*** WATCHDOG ERROR: the watchdog's read of its event FIFO ended without an event ***"
                status=$((status | 2))
            fi
            return 2
        }

        # Returns 0 if the given event has been read
        watchdog_got() {
            case "
$events
" in
                *"
event $1
"*) return 0 ;;
            esac
            return 1
        }

        # Returns 0 if SIGUSR1 cannot end the test, which is then sent it for the call stacks of its threads: QUnit has
        # reported that it has installed its handler in the test's own process, or, on Linux, the test catches the
        # signal with a signal handler, or every thread of the test blocks it (the masks in /proc/<pid>/task/*/status;
        # /proc is QORE_TEST_PROC if set, for the tests of run_tests.sh).  Otherwise the signal's default action would
        # end the test, without a core dump and without its call stacks.  The masks do not show a Qore handler: Qore's
        # signal thread waits for the signals that have one with sigwait(), and while it waits, Linux shows them as
        # unblocked in that thread, as it shows the signals that have none.
        usr1_cannot_end_test() {
            if watchdog_got "handler $test_pid"; then
                return 0
            fi
            proc=${QORE_TEST_PROC:-/proc}
            if [ ! -d "$proc/$test_pid/task" ]; then
                return 1
            fi
            # the number of SIGUSR1, which depends on the architecture
            usr1=1
            while [ $usr1 -le 64 ] && [ "`kill -l $usr1 2>/dev/null`" != USR1 ]; do
                usr1=$((usr1 + 1))
            done
            if [ $usr1 -gt 64 ]; then
                return 1
            fi
            # the masks are in hexadecimal, signal 1 in the lowest bit.  The bit's value in its digit (1, 2, 4 or 8) is
            # looked up, not computed with "^": a BusyBox awk built without math support (as Ubuntu's busybox package)
            # rejects the whole program for it, so the check failed and a test that handles SIGUSR1 was aborted at once,
            # without its call stacks
            cat "$proc/$test_pid"/task/*/status 2>/dev/null | awk -v sig=$usr1 '
                function has_signal(mask,   digit) {
                    digit = substr(mask, length(mask) - int((sig - 1) / 4), 1)
                    if (digit == "") {
                        return 0
                    }
                    return int((index("0123456789abcdef", tolower(digit)) - 1) / substr("1248", (sig - 1) % 4 + 1, 1)) % 2
                }
                $1 == "SigBlk:" { ++threads; if (has_signal($2)) ++blocked }
                $1 == "SigCgt:" && has_signal($2) { caught = 1 }
                END { exit !(threads > 0 && (caught || blocked == threads)) }'
        }

        # On Linux, prints the state of each thread of the test (its stat and wchan in /proc/<pid>/task/<tid>), the
        # pressure stall information of the machine (/proc/pressure) and the CPU and memory events of the test's cgroup
        # (QORE_TEST_CGROUP, by default its cgroup v2 directory), whatever of them can be read; the argument says when.  It
        # tells a thread that is runnable but not scheduled (R) from one that waits in the kernel (D, a page fault or
        # I/O) or sleeps (S), and the CPU time each one has used: a core shows only where each thread was (qore#5744:
        # a background compile thread that got no CPU for 445 s, at a user-mode address in LLVM).  It cannot fail the
        # watchdog: a file that ends or goes away while it is read is skipped.
        print_test_threads() {
            proc=${QORE_TEST_PROC:-/proc}
            if [ ! -d "$proc/$test_pid/task" ]; then
                return 0
            fi
            ticks=`getconf CLK_TCK 2>/dev/null`
            echo "*** threads of process $test_pid $1 (state, CPU time in 1/${ticks:-100} s, kernel wait channel) ***"
            for task in "$proc/$test_pid"/task/*; do
                stat=`cat "$task/stat" 2>/dev/null` || continue
                wchan=`cat "$task/wchan" 2>/dev/null`
                # the command name is between the first "(" and the last ")", and may contain either
                printf '%s\n' "$stat" | awk -v wchan="${wchan:-?}" '{
                    s = $0
                    first = index(s, "(")
                    last = 0
                    while ((i = index(substr(s, last + 1), ")")) > 0) {
                        last += i
                    }
                    if (!first || last <= first) {
                        next
                    }
                    n = split(substr(s, last + 2), f, " ")
                    if (n < 13) {
                        next
                    }
                    printf "    thread %s (%s): state %s, user %s, system %s, wchan %s\n", substr(s, 1, first - 2),
                        substr(s, first + 1, last - first - 1), f[1], f[12], f[13], wchan
                }' 2>/dev/null
            done
            for resource in cpu memory io; do
                if [ -r "$proc/pressure/$resource" ]; then
                    echo "    pressure $resource: `tr '\n' ' ' < "$proc/pressure/$resource" 2>/dev/null`"
                fi
            done
            # the test's own cgroup (v2: the "0::<path>" line), which is /sys/fs/cgroup itself in a container's cgroup
            # namespace
            cgroup=$QORE_TEST_CGROUP
            if [ -z "$cgroup" ]; then
                cgroup=/sys/fs/cgroup`awk -F: '$1 == "0" { print $3; exit }' "$proc/$test_pid/cgroup" 2>/dev/null`
            fi
            for file in cpu.stat memory.events; do
                if [ -r "$cgroup/$file" ]; then
                    echo "    cgroup $file: `tr '\n' ' ' < "$cgroup/$file" 2>/dev/null`"
                fi
            done
            return 0
        }

        # The watchdog's own process ID (a subshell's $$ is the runner's), for watchdog_check_test(): its parent is the
        # runner's shell while that shell runs; "sh -c" is its child, as a command substitution would run it in one more
        # subshell
        watchdog_self=""
        if sh -c 'echo $PPID' > "$TEST_EVENT_DIR/watchdog-self" 2>/dev/null; then
            read -r watchdog_self < "$TEST_EVENT_DIR/watchdog-self"
        fi
        rm -f "$TEST_EVENT_DIR/watchdog-self"

        # Returns 0 if test_pid is still the test's process: while the runner's shell runs (it is still the watchdog's
        # parent), its child that it has not waited for (test_is_shell_child()); and always a process with the test's
        # QORE_TEST_PROCESS_MARK (test_has_mark()).  A shell that is killed with SIGKILL (a kill by process ID) runs no
        # trap, so nothing stops its watchdog (see runner_signaled()) or tells it that the test has ended; at the
        # test's time limit, the test's process ID may then be another process's, which must not be signalled
        # (qore#6875: the watchdog subshell of a test of another run was killed with SIGKILL).  Once that shell has
        # ended, its process ID may be another shell's too, whose child may have the test's ID: only the mark counts
        # then.  So an orphaned test that hangs is still ended, unless its process shows no environment (macOS shows
        # none of a system binary, ps -E: a test that is a shell script, for example), which is then left running.
        # Otherwise this prints that nothing is sent, and the watchdog ends.  The runner's shell may wait for the test
        # between the check and the signal (it then writes "exit"): the ID would have to be taken by another process in
        # that instant.
        watchdog_check_test() {
            if [ -n "$watchdog_self" ] && test_is_shell_child $watchdog_self && test_is_shell_child $test_pid; then
                return 0
            fi
            if test_has_mark $test_pid "$TEST_PROCESS_MARK"; then
                return 0
            fi
            echo "*** process $test_pid is no longer the test: nothing is sent to it ***"
            exit $status
        }

        # Sends the given signal to the test, after the message given, if any, once watchdog_check_test() has passed
        watchdog_signal() {
            watchdog_check_test
            if [ -n "$2" ]; then
                echo "$2"
            fi
            kill -$1 $test_pid 2>/dev/null
        }

        watchdog_wait $limit exit
        # the test has ended, or the read failed before the time limit: the test is not ended for that (an
        # interrupted run stops the watchdog with "exit" too, in this phase or any later one; see runner_signaled())
        if [ $? -ne 1 ]; then
            exit $status
        fi
        status=$((status | 1))
        # the message printed with the timeout signal, if it is not sent after SIGUSR1
        timeout_message=""
        watchdog_check_test
        print_test_threads "at its time limit"
        if [ $TEST_SIGNALS_ENABLED -eq 0 ]; then
            timeout_message="*** the test exceeded its ${limit}s limit: SIG$TEST_TIMEOUT_SIGNAL sent (signal handling is \
disabled) ***"
        elif usr1_cannot_end_test; then
            watchdog_signal USR1 \
                "*** the test exceeded its ${limit}s limit: SIGUSR1 sent for the call stacks of its threads ***"
            if watchdog_wait $TEST_STACK_DUMP_WAIT dumped exit && watchdog_got exit; then
                exit $status
            fi
            # again after the wait for the call stacks, so that the CPU time each thread used meanwhile shows
            print_test_threads "before SIG$TEST_TIMEOUT_SIGNAL"
        else
            timeout_message="*** the test exceeded its ${limit}s limit: SIG$TEST_TIMEOUT_SIGNAL sent at once, as it \
does not handle SIGUSR1, which would end it without its call stacks or a core dump ***"
        fi
        watchdog_signal $TEST_TIMEOUT_SIGNAL "$timeout_message"
        if ! watchdog_wait 60 exit; then
            watchdog_signal KILL
        fi
        exit $status
    ) &
    watchdog_pid=$!
    watchdog_starting=0
    wait $test_pid
    WATCHDOG_TEST_EXIT=$?
    test_pid=""
    echo exit >&4
    wait $watchdog_pid
    watchdog_rc=$?
    watchdog_pid=""
    exec 4>&-
    rm -f "$TEST_EVENT_FIFO"
    case $watchdog_rc in
        0) ;;
        1) WATCHDOG_TIMED_OUT=1 ;;
        2) WATCHDOG_ERROR=1 ;;
        3) WATCHDOG_TIMED_OUT=1; WATCHDOG_ERROR=1 ;;
        *)
            WATCHDOG_ERROR=1
            watchdog_message "*** WATCHDOG ERROR: the watchdog ended with exit code $watchdog_rc ***"
            ;;
    esac
    # The test exceeded its time limit, and the watchdog has ended it (SIGUSR1 for its call stacks, then SIGABRT): the
    # processes it started are ended too, as nothing else would end them.  Those of a test that ended by itself are
    # left to the check for the processes a test leaves (see find_test_survivors()), which fails the test for them.
    if [ $WATCHDOG_TIMED_OUT -eq 1 ]; then
        end_test_group $TEST_PID
    fi
    test_group=""
    if [ $WATCHDOG_TIMED_OUT -eq 1 ]; then
        return 124
    fi
    return $WATCHDOG_TEST_EXIT
}

# Prints the header of a test (the arguments: the test and its number)
print_test_header() {
    if [ $PRINT_TEXT -eq 1 ]; then
        echo "====================================="
        echo "Running test ($2/$TEST_COUNT): $1"
        echo "-------------------------------------"
        echo "cmdline: $QORE $QORE_TEST_OPTS $1 $TEST_OUTPUT_FORMAT"
        echo "-------------------------------------"
    fi
}

# Prints the footer of a test (the arguments: the test and the seconds it took)
print_test_footer() {
    if [ $PRINT_TEXT -eq 1 ]; then
        # read by update-test-durations.py
        echo "Test time: $1: $2s"
        echo "-------------------------------------"
        echo
    fi
}

# Runs a test (the arguments: the test and its number), with its diagnostics when it fails; sets test_exit to the
# result: 0 if it passed.  The output of the test goes to stdout, or to TEST_LOG if set, which is then printed.
run_test() {
    test=$1
    i=$2
    # the test's own budget, if it declares one
    THIS_TEST_TIMEOUT=`sed -n 's/^[	 ]*#[	 ]*test-timeout:[	 ]*\([0-9][0-9]*\).*/\1/p' "$test" 2>/dev/null | head -1`
    if [ -z "$THIS_TEST_TIMEOUT" ]; then
        THIS_TEST_TIMEOUT=$TEST_TIMEOUT
    fi

    list_test_cores > "$TEST_CORES_BEFORE"
    touch "$TEST_START_MARKER"
    # given to the test, and so inherited by every process it starts, but not to the helpers that time it (see
    # find_test_survivors())
    TEST_PROCESS_MARK="$$.$i"
    TEST_ENV="env QORE_TEST_PROCESS_MARK=$TEST_PROCESS_MARK"
    # set by run_with_watchdog()
    WATCHDOG_TEST_EXIT=""
    WATCHDOG_TIMED_OUT=0
    WATCHDOG_ERROR=0
    TEST_PID=""
    # a parallel run writes the output of the test to its log (TEST_LOG), which is printed when the test has ended;
    # it is opened for appending, as the watchdog appends its messages to it too
    TEST_LOG_REDIRECT=""
    if [ -n "$TEST_LOG" ]; then
        : > "$TEST_LOG"
        TEST_LOG_REDIRECT='>> "$TEST_LOG" 2>&1'
    fi
    if [ $MEASURE_TIME -eq 1 ]; then
        if [ -n "$TIMEOUT_CMD" ]; then
            eval $TIMEOUT_CMD $TIMEOUT_SIGNAL_OPTS $THIS_TEST_TIMEOUT $TIME_CMD $TEST_ENV $QORE $QORE_TEST_OPTS $test $TEST_OUTPUT_FORMAT $TEST_LOG_REDIRECT
        else
            eval $TIME_CMD $TEST_ENV $QORE $QORE_TEST_OPTS $test $TEST_OUTPUT_FORMAT $TEST_LOG_REDIRECT
        fi
    else
        if [ -n "$TIMEOUT_CMD" ]; then
            run_with_watchdog $THIS_TEST_TIMEOUT $TEST_ENV $QORE $QORE_TEST_OPTS $test $TEST_OUTPUT_FORMAT
        elif [ -n "$TEST_LOG" ]; then
            $TEST_ENV $QORE $QORE_TEST_OPTS $test $TEST_OUTPUT_FORMAT >> "$TEST_LOG" 2>&1
        else
            $TEST_ENV $QORE $QORE_TEST_OPTS $test $TEST_OUTPUT_FORMAT
        fi
    fi
    test_exit=$?
    if [ -n "$TEST_LOG" ]; then
        cat "$TEST_LOG"
    fi

    # A test must end every process it starts: one still running keeps the output of the tests open, so that a CI job
    # running them does not end until its time limit, and it can interfere with the tests that follow
    find_test_survivors "$TEST_PROCESS_MARK"
    # a BusyBox timeout watcher is ended without failing the test: the command it watches, if still running, is a
    # survivor of its own; a process that has ended since it was found is not running
    TEST_LEFTOVERS=""
    TEST_LEFTOVER_LINES=""
    TEST_SURVIVOR_STATES=""
    if [ -n "$TEST_SURVIVORS" ]; then
        TEST_SURVIVOR_STATES=`qore_ci_running_processes $TEST_SURVIVORS`
    fi
    while read -r kind pid cmd; do
        case "$kind" in
            watcher)
                kill -KILL $pid 2>/dev/null
                ;;
            running)
                TEST_LEFTOVERS="$TEST_LEFTOVERS $pid"
                TEST_LEFTOVER_LINES="$TEST_LEFTOVER_LINES  pid $pid: $cmd
"
                ;;
        esac
    done <<EOF
$TEST_SURVIVOR_STATES
EOF
    if [ -n "$TEST_LEFTOVERS" ]; then
        echo "*** FAILURE: the test left processes running; they are killed: ***"
        printf '%s' "$TEST_LEFTOVER_LINES"
        kill -KILL $TEST_LEFTOVERS 2>/dev/null
        if [ $test_exit -eq 0 ]; then
            test_exit=1
        fi
    fi

    # The watchdog failed: the test's result is its own, but the run fails as a harness error, so that the failure is
    # not lost
    if [ $WATCHDOG_ERROR -eq 1 ]; then
        echo "*** WATCHDOG ERROR: run_tests.sh could not watch the test as it should (see above); the test's result" \
            "is its own, but the run fails ***"
    fi

    # GNU timeout returns 124; busybox timeout (Alpine) returns 143 (128+SIGTERM)
    if [ $test_exit -eq 124 ] || [ $test_exit -eq 143 ]; then
        if [ $WATCHDOG_TIMED_OUT -eq 1 ] && [ "$WATCHDOG_TEST_EXIT" -gt 128 ]; then
            echo "TIMEOUT: test exceeded ${THIS_TEST_TIMEOUT}s limit (it was ended by signal" \
                "`expr $WATCHDOG_TEST_EXIT - 128`)"
        else
            echo "TIMEOUT: test exceeded ${THIS_TEST_TIMEOUT}s limit"
        fi
        # the stacks of the threads of a test aborted by the watchdog or the timeout show where it hangs
        if { [ $WATCHDOG_TIMED_OUT -eq 1 ] || [ -n "$TIMEOUT_SIGNAL_OPTS" ]; } && command -v gdb > /dev/null 2>&1; then
            find_test_core
            TIMEOUT_CORE=$CORE_FILE
            if [ -n "$TIMEOUT_CORE" ]; then
                echo "*** Core dump of the hung test: $TIMEOUT_CORE - extracting the stacks of its threads ***"
                BT_FILE="$CORE_DIR/backtrace-`test_artifact_name "$test"`-timeout.txt"
                gdb -batch -ex "thread apply all bt" -ex "quit" "$QORE" "$TIMEOUT_CORE" > "$BT_FILE" 2>&1
                head -500 "$BT_FILE"
            fi
        fi
    fi

    if [ $test_exit -ne 0 ]; then
        # BusyBox reports its deliberate timeout termination as SIGTERM (143). It is already
        # recorded above; rerunning that hung test under gdb would lose the timeout entirely.
        # Capture crash diagnostics only for unexpected signals.
        if [ $test_exit -gt 128 ] && [ $test_exit -ne 143 ]; then
            SIG_NUM=`expr $test_exit - 128`
            echo "*** CRASH: test killed by signal $SIG_NUM (exit code $test_exit) ***"
            find_test_core
            TEST_ARTIFACT_NAME=$(test_artifact_name "$test")
            if [ -n "$CORE_FILE" ] && command -v gdb > /dev/null 2>&1; then
                echo "*** Core dump found: $CORE_FILE - extracting backtrace ***"
                BT_FILE="$CORE_DIR/backtrace-${TEST_ARTIFACT_NAME}.txt"
                # Write the whole backtrace to the file first, then show the head of the file: piping
                # gdb through "tee | head" makes head close the pipe after its limit, and the
                # resulting SIGPIPE kills tee and gdb, so the saved artifact is truncated too.  The
                # crashing thread is printed LAST by "thread apply all bt", so it is exactly what
                # such a truncation loses; dump it first so it survives both the file and the head.
                gdb -batch -ex "bt full" -ex "thread apply all bt full" -ex "quit" \
                    "$QORE" "$CORE_FILE" > "$BT_FILE" 2>&1
                head -500 "$BT_FILE"
                # Move core to artifact directory for CI collection (don't delete)
                case "$CORE_FILE" in
                    "$CORE_DIR"/*|"$CORE_DIR_ABS"/*) ;;
                    *) cp "$CORE_FILE" "$CORE_DIR/" 2>/dev/null && rm -f "$CORE_FILE" || true ;;
                esac
            elif [ -n "$CORE_FILE" ] && command -v lldb > /dev/null 2>&1; then
                echo "*** Core dump found: $CORE_FILE - extracting backtrace ***"
                BT_FILE="$CORE_DIR/backtrace-${TEST_ARTIFACT_NAME}.txt"
                # the crashing thread first, as the head of the file is what the log shows
                lldb --batch -c "$CORE_FILE" "$QORE" -o "thread backtrace" -o "thread backtrace all" \
                    > "$BT_FILE" 2>&1
                head -500 "$BT_FILE"
                # a macOS core holds the whole address space (gigabytes); the macOS CI machine cannot upload
                # artifacts, and a few cores would fill its disk
                if [ -n "$CI" ]; then
                    rm -f "$CORE_FILE"
                fi
            elif command -v gdb > /dev/null 2>&1; then
                echo "*** No core dump found; re-running under gdb to try to capture backtrace ***"
                BT_FILE="$CORE_DIR/backtrace-${TEST_ARTIFACT_NAME}.txt"
                # Filter out thread creation/exit noise (both glibc [New Thread] and musl [New LWP] formats)
                gdb -batch -ex run -ex "bt full" -ex "thread apply all bt full" -ex quit \
                    --args $QORE $QORE_TEST_OPTS $test $TEST_OUTPUT_FORMAT 2>&1 \
                    | grep -v '^\[New Thread\|^\[Thread.*exited\]\|^\[New LWP\|^\[LWP.*exited\]\|^\[Detaching' \
                    > "$BT_FILE"
                head -500 "$BT_FILE"
            elif command -v lldb > /dev/null 2>&1; then
                # macOS: no gdb and no core files by default; the crash report names the reason for a
                # SIGABRT raised without a message (e.g. a libpthread or malloc integrity check)
                if [ -d "$HOME/Library/Logs/DiagnosticReports" ]; then
                    REPORT=$(ls -t "$HOME"/Library/Logs/DiagnosticReports/qore*.ips 2>/dev/null | head -1)
                    # tests that run at the same time: the newest report of the test's process
                    if [ "$TEST_JOBS" -gt 1 ] && [ -n "$TEST_PID" ]; then
                        REPORT=$(ls -t "$HOME"/Library/Logs/DiagnosticReports/qore*.ips 2>/dev/null \
                            | while read -r r; do if grep -q "\"pid\" *: *$TEST_PID," "$r"; then echo "$r"; break; fi; done)
                    fi
                    # BSD stat: GNU coreutils may be first in PATH; only a report written just now belongs to this test
                    if [ -n "$REPORT" ] && [ $(( $(date +%s) - $(/usr/bin/stat -f %m "$REPORT") )) -lt 120 ]; then
                        echo "*** Crash report: $REPORT ***"
                        head -200 "$REPORT"
                        cp "$REPORT" "$CORE_DIR/" 2>/dev/null || true
                    fi
                fi
                echo "*** No core dump found; re-running under lldb to try to capture backtrace ***"
                BT_FILE="$CORE_DIR/backtrace-${TEST_ARTIFACT_NAME}.txt"
                LLDB=lldb
                # a non-interactive session cannot get permission to debug a process, which root does not
                # need; only on disposable CI machines, as the test is then run as root
                if [ -n "$CI" ] && sudo -n true 2>/dev/null; then
                    LLDB="sudo -n -E lldb"
                fi
                $LLDB --batch -o run -k "thread backtrace all" -k quit \
                    -- $QORE $QORE_TEST_OPTS $test $TEST_OUTPUT_FORMAT > "$BT_FILE" 2>&1
                head -500 "$BT_FILE"
            else
                echo "*** No core dump found and neither gdb nor lldb is available ***"
            fi
        fi
    fi

}

# Counts the result of a test (the arguments: the test, its result and its WATCHDOG_ERROR); it is counted by the runner
# itself, as the test of a parallel run runs in a shell of its own (see run_tests_parallel())
count_test_result() {
    if [ "$2" -eq 0 ]; then
        PASSED_TEST_COUNT=`expr $PASSED_TEST_COUNT + 1`
    else
        FAILED_TEST_COUNT=`expr $FAILED_TEST_COUNT + 1`
        FAILED_TESTS="$FAILED_TESTS $1"
    fi
    if [ "$3" = 1 ]; then
        WATCHDOG_ERROR_TESTS="$WATCHDOG_ERROR_TESTS $1"
        WATCHDOG_ERROR_COUNT=`expr $WATCHDOG_ERROR_COUNT + 1`
        if [ "$2" -eq 0 ]; then
            WATCHDOG_ERROR_PASSED_COUNT=`expr $WATCHDOG_ERROR_PASSED_COUNT + 1`
        fi
    fi
}

# Runs the tests TEST_JOBS at a time (see TEST_JOBS above): the exclusive tests first, one by one, then the others,
# with at most one serial test at a time, each lane longest first by the durations of the tests (see plan_tests()).
# Each test runs in a background shell with a directory of its own (named by the test's number), and writes its output
# there; when the shell has ended, however it ended, the number of the test is written to a FIFO, and the runner, which
# reads the FIFO, prints the output and the result and starts the next test.
run_tests_parallel() {
    RUN_DIR=`mktemp -d "${TMPDIR:-/tmp}/qore-run-tests.XXXXXX"` || exit 1
    RUN_DIR=`cd "$RUN_DIR" && pwd -P`
    RUN_PIDS=""
    # a run that ends early (a signal or an error) ends its tests too
    trap 'stop_parallel_tests; remove_runner_files' 0
    trap 'parallel_runner_signaled HUP 1' HUP
    trap 'parallel_runner_signaled INT 2' INT
    trap 'parallel_runner_signaled TERM 15' TERM
    if ! mkfifo "$RUN_DIR/done"; then
        echo "ERROR: cannot make the FIFO of the parallel run in $RUN_DIR" >&2
        exit 1
    fi
    # opened for reading and writing, so that it never reaches its end while tests run
    exec 5<>"$RUN_DIR/done"

    # the lanes, longest first
    # "<number> <test>" lines
    TEST_ORDER=`plan_tests 1 order | awk -F '\t' '{ print $3 " " $4 }'`
    SERIAL_TESTS=`grep -l '^# test-parallel: serial' $TESTS 2>/dev/null`
    EXCLUSIVE_TESTS=`grep -l '^# test-parallel: exclusive' $TESTS 2>/dev/null`
    PENDING_FREE=""
    PENDING_SERIAL=""
    PENDING_EXCLUSIVE=""
    while read -r number test; do
        case "
$EXCLUSIVE_TESTS
" in
            *"
$test
"*) PENDING_EXCLUSIVE="$PENDING_EXCLUSIVE$number $test
"; continue ;;
        esac
        case "
$SERIAL_TESTS
" in
            *"
$test
"*) PENDING_SERIAL="$PENDING_SERIAL$number $test
"; continue ;;
        esac
        PENDING_FREE="$PENDING_FREE$number $test
"
    done <<EOF
$TEST_ORDER
EOF
    echo "Running $TEST_COUNT tests, $TEST_JOBS at a time ($(printf '%s' "$PENDING_SERIAL" | grep -c .) serial," \
        "$(printf '%s' "$PENDING_EXCLUSIVE" | grep -c .) exclusive)"
    echo

    RUNNING=0
    SERIAL_RUNNING=0
    EXCLUSIVE_RUNNING=0
    DONE_COUNT=0
    while :; do
        # start the tests that can start
        while [ $RUNNING -lt $TEST_JOBS ] && [ $EXCLUSIVE_RUNNING -eq 0 ]; do
            next=""
            lane=""
            if [ -n "$PENDING_EXCLUSIVE" ]; then
                # alone: once the tests that run have ended
                if [ $RUNNING -eq 0 ]; then
                    next=${PENDING_EXCLUSIVE%%"
"*}
                    PENDING_EXCLUSIVE=${PENDING_EXCLUSIVE#*"
"}
                    lane=exclusive
                fi
            elif [ -n "$PENDING_SERIAL" ] && [ $SERIAL_RUNNING -eq 0 ]; then
                next=${PENDING_SERIAL%%"
"*}
                PENDING_SERIAL=${PENDING_SERIAL#*"
"}
                lane=serial
            elif [ -n "$PENDING_FREE" ]; then
                next=${PENDING_FREE%%"
"*}
                PENDING_FREE=${PENDING_FREE#*"
"}
                lane=free
            fi
            if [ -z "$next" ]; then
                break
            fi
            set -- $next
            number=$1
            next=$2
            dir="$RUN_DIR/$number"
            mkdir "$dir" || exit 1
            echo "$next" > "$dir/test"
            echo "$lane" > "$dir/lane"
            # The test runs in an inner shell, which writes its result, inside an outer one, which keeps the result
            # once the inner one has ended.  A reporter reads the standard output of the outer shell, a pipe that
            # nothing else holds, to its end, which comes when the outer shell has ended, however it ended (a shell
            # that is killed writes nothing), and then reports the test; so the runner never waits for a test that is
            # gone, and a test whose outer shell ended before it kept the result has none.  The test does not get the
            # FIFO.
            # The shells around the inner one ignore SIGHUP and SIGTERM, which a CI executor may send to every
            # process of the job: they end when the inner one has ended its test (see stop_parallel_tests()).
            (
                trap - 0 INT
                trap '' HUP TERM
                (
                    (
                        exec > "$dir/output" 2>&1 5>&-
                        # the test's own files (see find_test_core(), run_with_watchdog() and runner_signaled())
                        TEST_START_MARKER="$dir/start"
                        TEST_CORES_BEFORE="$dir/cores"
                        TEST_EVENT_DIR="$dir"
                        TEST_EVENT_FIFO="$dir/events"
                        TEST_LOG="$dir/log"
                        # SIGINT cannot be trapped here (a shell run in the background ignores it for good); the
                        # runner sends SIGTERM for it
                        trap 'runner_signaled HUP 1 job' HUP
                        trap 'runner_signaled TERM 15 job' TERM
                        # the process ID of this shell, for stop_parallel_tests(), once its traps are set; a run that is
                        # stopped before it is recorded is found here
                        sh -c 'echo $PPID' > "$dir/shell.new" && RUNNER_SHELL_PID=`cat "$dir/shell.new"` \
                            && mv "$dir/shell.new" "$dir/shell"
                        if [ -f "$RUN_DIR/stopped" ]; then
                            exit 143
                        fi
                        print_test_header "$next" $number
                        test_start=`date +%s`
                        test_exit=1
                        run_test "$next" $number
                        print_test_footer "$next" `expr \`date +%s\` - $test_start`
                        rm -f "$dir/shell"
                        echo "$test_exit $WATCHDOG_ERROR" > "$dir/exit"
                    )
                    if [ -f "$dir/exit" ]; then
                        mv "$dir/exit" "$dir/result"
                    fi
                ) | {
                    cat > /dev/null
                    echo "$number" >&5
                }
            ) &
            RUN_PIDS="$RUN_PIDS $!"
            echo "$!" > "$dir/pid"
            RUNNING=`expr $RUNNING + 1`
            case $lane in
                serial) SERIAL_RUNNING=1 ;;
                exclusive) EXCLUSIVE_RUNNING=1 ;;
            esac
        done
        if [ $RUNNING -eq 0 ]; then
            break
        fi
        # the next test that ends
        if ! IFS= read -r number <&5; then
            echo "ERROR: the parallel run lost its FIFO" >&2
            exit 1
        fi
        dir="$RUN_DIR/$number"
        pid=`cat "$dir/pid"`
        wait $pid 2>/dev/null
        RUN_PIDS=`for p in $RUN_PIDS; do [ "$p" = "$pid" ] || echo $p; done`
        test=`cat "$dir/test"`
        result=""
        watchdog_error=0
        if [ -f "$dir/result" ]; then
            read -r result watchdog_error < "$dir/result"
        fi
        if [ -z "$result" ]; then
            result=1
            echo "*** the shell that ran $test ended without its result ***" >> "$dir/output"
        fi
        cat "$dir/output"
        count_test_result "$test" "$result" "$watchdog_error"
        DONE_COUNT=`expr $DONE_COUNT + 1`
        case `cat "$dir/lane"` in
            serial) SERIAL_RUNNING=0 ;;
            exclusive) EXCLUSIVE_RUNNING=0 ;;
        esac
        rm -rf "$dir"
        RUNNING=`expr $RUNNING - 1`
    done
    exec 5>&-
    # the failed tests in the order of their names
    FAILED_TESTS=`for test in $FAILED_TESTS; do echo "$test"; done | sort`
    WATCHDOG_ERROR_TESTS=`for test in $WATCHDOG_ERROR_TESTS; do echo "$test"; done | sort`
}

# Ends the tests of a parallel run that still run, when it ends early: each one's shell is sent SIGTERM, which ends
# its test as runner_signaled() does in a run of one test at a time, within TEST_INTERRUPT_WAIT seconds, and the shells
# are waited for; a shell that has not recorded its process ID yet finds RUN_DIR/stopped when it does, and ends
# without starting its test.  Then the output of each one is printed, and the files of the run are removed.  A shell
# removes its process ID once its test has ended and before it ends, so that only a shell that runs is signalled; a
# process that took its ID in the instant between the check and the signal would get SIGTERM, never SIGKILL.
stop_parallel_tests() {
    if [ -z "$RUN_DIR" ]; then
        return
    fi
    : > "$RUN_DIR/stopped"
    for shell in "$RUN_DIR"/*/shell; do
        if [ -f "$shell" ]; then
            kill -TERM `cat "$shell"` 2>/dev/null
        fi
    done
    if [ -n "$RUN_PIDS" ]; then
        wait $RUN_PIDS 2>/dev/null
    fi
    RUN_PIDS=""
    for output in "$RUN_DIR"/*/output; do
        if [ -f "$output" ]; then
            cat "$output"
        fi
    done
    rm -rf "$RUN_DIR"
    RUN_DIR=""
}

# Ends a parallel run for a signal (the name and the number), as runner_signaled() ends a run of one test at a time
parallel_runner_signaled() {
    trap '' HUP INT TERM
    # the words are split as usual: the runner waits for the next test that ends with "IFS= read", and a trap that
    # interrupts it runs with that IFS in bash, where "wait $RUN_PIDS" was then a wait for one process with a name made
    # of all their IDs, which returned at once
    unset IFS
    stop_parallel_tests
    remove_runner_files
    trap - 0 $1
    kill -$1 $$
    # if the signal did not end the shell
    exit $((128 + $2))
}

# Run tests.
if [ "$TEST_JOBS" -le 1 ]; then
    i=1
    for test in $TESTS; do
        print_test_header "$test" $i
        test_start=`date +%s`
        TEST_LOG=""
        run_test "$test" $i
        count_test_result "$test" $test_exit $WATCHDOG_ERROR
        print_test_footer "$test" `expr \`date +%s\` - $test_start`
        i=`expr $i + 1`
    done
else
    run_tests_parallel
fi

# Every test has a result: a run that lost one (a parallel run whose shell ended without writing it is counted as a
# failure) must not pass
if [ `expr $PASSED_TEST_COUNT + $FAILED_TEST_COUNT` -ne $TEST_COUNT ]; then
    echo "ERROR: $TEST_COUNT tests were selected, but $PASSED_TEST_COUNT passed and $FAILED_TEST_COUNT failed" >&2
    FAILED_TEST_COUNT=`expr $TEST_COUNT - $PASSED_TEST_COUNT`
    if [ $FAILED_TEST_COUNT -lt 1 ]; then
        FAILED_TEST_COUNT=1
    fi
fi

# Print test summary.
# A test whose watchdog failed fails the run as well, as a harness error
RUN_FAILURE_COUNT=`expr $FAILED_TEST_COUNT + $WATCHDOG_ERROR_PASSED_COUNT`
if [ $PRINT_TEXT -eq 1 ]; then
    TESTING_RESULT=""
    if [ $RUN_FAILURE_COUNT -eq 0 ]; then
        TESTING_RESULT="Success."
    else
        TESTING_RESULT="Failure."
    fi

    echo; echo "*************************************"
    echo "TESTING RESULT: $TESTING_RESULT"
    echo "Passed $PASSED_TEST_COUNT out of $TEST_COUNT tests. $FAILED_TEST_COUNT tests failed."

    if [ $FAILED_TEST_COUNT -ne 0 ]; then
        echo "Failed tests:"
        for test in $FAILED_TESTS; do
            echo $test
        done
    fi

    if [ $WATCHDOG_ERROR_COUNT -ne 0 ]; then
        echo "Harness errors: the watchdog of $WATCHDOG_ERROR_COUNT tests failed (WATCHDOG ERROR in their output):"
        for test in $WATCHDOG_ERROR_TESTS; do
            echo $test
        done
    fi

    echo "*************************************"
fi

# the exit status is the number of failed tests, and of passed tests whose watchdog failed; it has 8 bits, so 256
# failed tests would exit with 0 as if every test had passed
if [ $RUN_FAILURE_COUNT -gt 255 ]; then
    exit 255
fi
exit $RUN_FAILURE_COUNT
