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
  echo "  -P         Run only performance/stress tests."
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
  echo "                           that exceeds it is first sent SIGUSR1 (QUnit tests print the call stacks of"
  echo "                           their threads), unless QORE_TEST_OPTS disables signal handling (-b)."
  echo "  QORE_TEST_STACK_DUMP_WAIT"
  echo "                           Seconds to wait for those stacks before the test is ended (default: 30)."
  echo "  CI_NODE_INDEX            Shard index (1-based) for parallel test execution."
  echo "  CI_NODE_TOTAL            Total number of shards for parallel test execution; the shards are balanced by"
  echo "                           the durations of the tests in examples/test/docker_test/test-durations.tsv."
  echo "  QORE_TEST_DURATION_PROFILE"
  echo "                           The column of test-durations.tsv used (default: CI_JOB_NAME without its shard)."
  echo "  QORE_TEST_DURATIONS_FILE The durations file (default: examples/test/docker_test/test-durations.tsv)."
  echo "  QORE_EXCLUDE_PERF_TESTS  Set to '1' to exclude performance tests (same as -E)."
  echo "  QORE_PERF_TESTS_ONLY    Set to '1' to run only performance tests (same as -P)."
  echo "  QORE_TEST_EXCLUDE_EXEC_MODE_CHILDREN"
  echo "                           Set to '1' to leave out the tests that declare '# test-exec-mode: children'"
  echo "                           (needs --exec-mode in QORE_TEST_OPTS): their child processes set their own"
  echo "                           execution mode, so a run in the default mode makes the same checks."
  echo "  QORE_TEST_QMOD_DIR       Directory with AOT qmods to prefer before ./qlib."
  echo "  QORE_TEST_SOURCE_MODULES Set to '1' to ignore the default build qmod directory."
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
    FILTERED=""
    for test in $TESTS; do
        if is_perf_test "$test"; then
            FILTERED="$FILTERED $test"
        fi
    done
    TESTS="$FILTERED"
fi

# The tests that declare "# test-exec-mode: children" make all their checks in child processes that set their own
# execution mode (--exec-mode) with ExecModeHarness.qm, so the execution mode of the test itself changes nothing they
# check: a run whose tests all get an execution mode (an --exec-mode option in QORE_TEST_OPTS) leaves them out when
# QORE_TEST_EXCLUDE_EXEC_MODE_CHILDREN=1, as a run in the default mode runs the same child processes.  A harness test
# with checks of its own declares "# test-exec-mode: parent" and always runs.
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

# Sets TEST_SURVIVORS to the processes the current test started that are still running (Linux only).  Every process a
# test starts, and every process those start, inherits QORE_TEST_PROCESS_MARK in its environment, also when it
# leaves the test's session or is adopted by another parent, so a process that still has it when the test has ended
# was not ended by the test.  Any other process that holds the output of the tests as its standard output or error,
# such as one a test started with an environment of its own, is one too.
find_test_survivors() {
    TEST_SURVIVORS=""
    if [ ! -r /proc/self/environ ]; then
        return
    fi
    # the output of a test of a parallel run is its log (TEST_LOG, an absolute path without spaces)
    test_outputs="$TEST_OUTPUT_PIPES"
    if [ -n "$TEST_LOG" ]; then
        test_outputs="$TEST_LOG"
    fi
    if [ -n "$test_outputs" ]; then
        # the commands that look for them must not hold the output themselves
        TEST_SURVIVORS=`{ ls -l /proc/[0-9]*/fd/1 /proc/[0-9]*/fd/2 | awk -v pipes="$test_outputs" \
            -v own="$RUN_TESTS_ANCESTORS" 'BEGIN {
                n = split(pipes, list, " ")
                for (i = 1; i <= n; ++i) {
                    pipe[list[i]] = 1
                }
                n = split(own, list, " ")
                for (i = 1; i <= n; ++i) {
                    skip[list[i]] = 1
                }
            }
            $(NF - 1) == "->" && ($NF in pipe) {
                split($(NF - 2), path, "/")
                if (!(path[3] in skip) && !(path[3] in seen)) {
                    seen[path[3]] = 1
                    printf "%s ", path[3]
                }
            }'; } 2>/dev/null`
    fi
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
if command -v timeout >/dev/null 2>&1; then
    TIMEOUT_CMD=timeout
elif command -v gtimeout >/dev/null 2>&1; then
    TIMEOUT_CMD=gtimeout
else
    TIMEOUT_CMD=""
fi

# GNU timeout aborts a test that exceeds its time limit, so the test dumps core and the stacks of its threads
# show where it hangs; BusyBox timeout (Alpine) keeps SIGTERM, as the exit code of an abort there would be
# taken for a crash
TIMEOUT_SIGNAL_OPTS=""
if [ -n "$TIMEOUT_CMD" ] && $TIMEOUT_CMD --version 2>/dev/null | grep -q GNU; then
    TIMEOUT_SIGNAL_OPTS="-s ABRT -k 60"
fi

# A separate marker per runner also permits concurrent test shards.
TEST_START_MARKER=$(mktemp "$CORE_DIR_ABS/test-start.XXXXXX") || exit 1
# the core files there were when the current test started (see find_test_core())
TEST_CORES_BEFORE=$(mktemp "$CORE_DIR_ABS/test-cores.XXXXXX") || exit 1
# the FIFO through which a test and run_tests.sh report events to the watchdog of the test (see run_with_watchdog())
TEST_EVENT_FIFO="$CORE_DIR_ABS/test-events.$$"
trap 'rm -f "$TEST_START_MARKER" "$TEST_CORES_BEFORE" "$TEST_EVENT_FIFO"' 0

# A test that exceeds its time limit is first sent SIGUSR1, unless signal handling is disabled (where SIGUSR1 would
# end it without a core dump): a QUnit test then prints the call stacks of all its threads and reports it through
# the event FIFO.  Once it has reported it, has ended, or QORE_TEST_STACK_DUMP_WAIT seconds (default 30) have
# passed, the test is sent the signal that ends a hung test as before (SIGABRT with GNU timeout, so that it dumps
# core, SIGTERM otherwise), and SIGKILL 60 seconds later.
TEST_STACK_DUMP_WAIT=${QORE_TEST_STACK_DUMP_WAIT:-30}
TEST_TIMEOUT_SIGNAL=TERM
if [ -n "$TIMEOUT_SIGNAL_OPTS" ]; then
    TEST_TIMEOUT_SIGNAL=ABRT
fi
TEST_SIGNALS_ENABLED=1
for opt in $QORE_TEST_OPTS; do
    case "$opt" in
        -b|--disable-signals) TEST_SIGNALS_ENABLED=0 ;;
    esac
done

# Prints the next event from the event FIFO (open on fd 4) if one arrives within the given number of seconds
read_test_event() {
    $TIMEOUT_CMD "$1" sh -c 'IFS= read -r event && echo "$event"' <&4 2>/dev/null
}

# Runs a test (the arguments after the time limit) with its time limit; returns the test's exit code, or 124 if it
# exceeded its time limit
run_with_watchdog() {
    limit=$1
    shift
    rm -f "$TEST_EVENT_FIFO"
    if ! mkfifo "$TEST_EVENT_FIFO"; then
        $TIMEOUT_CMD $TIMEOUT_SIGNAL_OPTS $limit "$@"
        return $?
    fi
    # opened for reading and writing, so that neither the writes below nor the test's block
    exec 4<>"$TEST_EVENT_FIFO"
    if [ -n "$TEST_LOG" ]; then
        QORE_TEST_EVENT_FIFO="$TEST_EVENT_FIFO" "$@" 4>&- >> "$TEST_LOG" 2>&1 &
    else
        QORE_TEST_EVENT_FIFO="$TEST_EVENT_FIFO" "$@" 4>&- &
    fi
    test_pid=$!
    # the process of the test (env executes qore), whose core is found by its process ID (see find_test_core())
    TEST_PID=$test_pid
    (
        # the watchdog's messages go where the test's output goes
        if [ -n "$TEST_LOG" ]; then
            exec >> "$TEST_LOG" 2>&1
        fi
        event=`read_test_event $limit`
        if [ "$event" = exit ]; then
            exit 0
        fi
        if [ $TEST_SIGNALS_ENABLED -eq 1 ]; then
            echo "*** the test exceeded its ${limit}s limit: SIGUSR1 sent for the call stacks of its threads ***"
            kill -USR1 $test_pid 2>/dev/null
            event=`read_test_event $TEST_STACK_DUMP_WAIT`
            if [ "$event" = exit ]; then
                exit 1
            fi
        fi
        kill -$TEST_TIMEOUT_SIGNAL $test_pid 2>/dev/null
        event=`read_test_event 60`
        if [ "$event" != exit ]; then
            kill -KILL $test_pid 2>/dev/null
        fi
        exit 1
    ) &
    watchdog_pid=$!
    wait $test_pid
    test_rc=$?
    echo exit >&4
    wait $watchdog_pid
    timed_out=$?
    exec 4>&-
    rm -f "$TEST_EVENT_FIFO"
    if [ $timed_out -ne 0 ]; then
        return 124
    fi
    return $test_rc
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

    # GNU timeout returns 124; busybox timeout (Alpine) returns 143 (128+SIGTERM)
    if [ $test_exit -eq 124 ] || [ $test_exit -eq 143 ]; then
        echo "TIMEOUT: test exceeded ${THIS_TEST_TIMEOUT}s limit"
        # the stacks of the threads of a test aborted by the timeout show where it hangs
        if [ -n "$TIMEOUT_SIGNAL_OPTS" ] && command -v gdb > /dev/null 2>&1; then
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

# Counts the result of a test (the arguments: the test and its result)
count_test_result() {
    if [ "$2" -eq 0 ]; then
        PASSED_TEST_COUNT=`expr $PASSED_TEST_COUNT + 1`
    else
        FAILED_TEST_COUNT=`expr $FAILED_TEST_COUNT + 1`
        FAILED_TESTS="$FAILED_TESTS $1"
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
    # a run that ends early (a signal) ends its tests too
    trap 'kill $RUN_PIDS 2>/dev/null; rm -rf "$RUN_DIR"; rm -f "$TEST_START_MARKER" "$TEST_CORES_BEFORE"' 0
    trap 'exit 130' INT
    trap 'exit 143' TERM
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
            (
                trap - 0 INT TERM
                (
                    (
                        exec > "$dir/output" 2>&1 5>&-
                        # the test's own files (see find_test_core() and run_with_watchdog())
                        TEST_START_MARKER="$dir/start"
                        TEST_CORES_BEFORE="$dir/cores"
                        TEST_EVENT_FIFO="$dir/events"
                        TEST_LOG="$dir/log"
                        print_test_header "$next" $number
                        test_start=`date +%s`
                        test_exit=1
                        run_test "$next" $number
                        print_test_footer "$next" `expr \`date +%s\` - $test_start`
                        echo "$test_exit" > "$dir/exit"
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
        result=`cat "$dir/result" 2>/dev/null`
        if [ -z "$result" ]; then
            result=1
            echo "*** the shell that ran $test ended without its result ***" >> "$dir/output"
        fi
        cat "$dir/output"
        count_test_result "$test" "$result"
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
}

# Run tests.
if [ "$TEST_JOBS" -le 1 ]; then
    i=1
    for test in $TESTS; do
        print_test_header "$test" $i
        test_start=`date +%s`
        TEST_LOG=""
        run_test "$test" $i
        count_test_result "$test" $test_exit
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
if [ $PRINT_TEXT -eq 1 ]; then
    TESTING_RESULT=""
    if [ $FAILED_TEST_COUNT -eq 0 ]; then
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

    echo "*************************************"
fi

# the exit status is the number of failed tests; it has 8 bits, so 256 failed tests would exit with 0 as if every
# test had passed
if [ $FAILED_TEST_COUNT -gt 255 ]; then
    exit 255
fi
exit $FAILED_TEST_COUNT
