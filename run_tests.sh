#!/bin/sh

print_usage () {
  echo "Usage: run_tests.sh [OPTIONS]"
  echo "Run qore tests."
  echo
  echo "  -d <dir>   Run only specified tests (as found in $BASE_TEST_PATH)."
  echo "  -j         Use --format=junit option for the tests, making them print JUnit output."
  echo "  -E         Exclude performance/stress tests (matched by *Perf* or PipelineMemory pattern)."
  echo "  -P         Run only performance/stress tests."
  echo "  -t         Measure execution time of the tests."
  echo "  -v         Use --format=plain option for the tests, making them print one statement per each test case."
  echo
  echo "Environment variables:"
  echo "  QORE_TEST_OPTS           Additional options to pass to qore (e.g., '-penable-debug')."
  echo "  CI_NODE_INDEX            Shard index (1-based) for parallel test execution."
  echo "  CI_NODE_TOTAL            Total number of shards for parallel test execution."
  echo "  QORE_EXCLUDE_PERF_TESTS  Set to '1' to exclude performance tests (same as -E)."
  echo "  QORE_PERF_TESTS_ONLY    Set to '1' to run only performance tests (same as -P)."
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

# Support environment variables for CI integration
if [ "${QORE_EXCLUDE_PERF_TESTS}" = "1" ]; then
    PERF_EXCLUDE=1
fi
if [ "${QORE_PERF_TESTS_ONLY}" = "1" ]; then
    PERF_ONLY=1
fi

while getopts ":d:jvtEP" opt; do
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

if [ $PERF_EXCLUDE -eq 1 ] && [ $PERF_ONLY -eq 1 ]; then
    echo "Cannot use -E and -P together." >&2
    exit 1
fi

# If no test dirs were specified, run all the tests
if [ -z "$TEST_DIRS" ]; then
    #TEST_DIRS="$BASE_TEST_PATH ./modules"
    TEST_DIRS="$BASE_TEST_PATH"
fi

QORE=""
QR=""
LIBQORE=""
QORE_LIB_PATH="./lib/.libs:./qlib:$LD_LIBRARY_PATH"

# Allow callers to override binaries (ex: debug build output).
if [ -n "$QORE_BINARY" ]; then
    QORE="$QORE_BINARY"
    QORE_DIR=`dirname "$QORE"`
    if [ -f "$QORE_DIR/libqore.so" ]; then
        LIBQORE="$QORE_DIR/libqore.so"
    elif [ -f "$QORE_DIR/libqore.dylib" ]; then
        LIBQORE="$QORE_DIR/libqore.dylib"
    fi
    if [ -f "$QORE_DIR/qr" ]; then
        QR="$QORE_DIR/qr"
    fi
fi

# Allow explicit override for libqore if needed.
if [ -n "$LIBQORE_BINARY" ]; then
    LIBQORE="$LIBQORE_BINARY"
fi

# Test that qore is built.
if [ -z "$QORE" ] && [ -s "./.libs/qore" ] && [ -f "./qore" ] && [ -f "./lib/.libs/libqore.so" -o "./lib/.libs/libqore.dylib" ]; then
    if [ -f "./lib/.libs/libqore.so" ]; then
        LIBQORE="./lib/.libs/libqore.so"
    elif [ -f "./lib/.libs/libqore.dylib" ]; then
        LIBQORE="./lib/.libs/libqore.dylib"
    fi
    QORE="./.libs/qore"
    QR="./.libs/qr"
else
    if [ -z "$QORE" ]; then
        for D in `ls -d */`; do
            d=`echo ${D%%/}`
            if [ -f "$d/CMakeCache.txt" ] && [ -f "$d/qore" ] && [ -f "$d/libqore.so" -o "$d/libqore.dylib" ]; then
                if [ -f "$d/libqore.so" ]; then
                    LIBQORE="$d/libqore.so"
                elif [ -f "$d/libqore.dylib" ]; then
                    LIBQORE="$d/libqore.dylib"
                fi
                QORE="$d/qore"
                QR="$d/qr"
                break
            fi
        done
    fi
fi

if [ -z "$QORE" ] || [ -z "$LIBQORE" ]; then
    echo "Qore is not built. Exiting."
    exit 1
fi

QORE_DIR=`dirname "$QORE"`
BUILD_MODULE_DIRS=""
if [ -d "$QORE_DIR/modules" ]; then
    for moddir in "$QORE_DIR/modules"/*; do
        if [ -d "$moddir" ]; then
            if [ -z "$BUILD_MODULE_DIRS" ]; then
                BUILD_MODULE_DIRS="$moddir"
            else
                BUILD_MODULE_DIRS="$BUILD_MODULE_DIRS:$moddir"
            fi
        fi
    done
fi

export LD_LIBRARY_PATH=$QORE_LIB_PATH
if [ -n "$BUILD_MODULE_DIRS" ]; then
    export QORE_MODULE_DIR=$BUILD_MODULE_DIRS:./qlib:$QORE_MODULE_DIR
else
    export QORE_MODULE_DIR=./qlib:$QORE_MODULE_DIR
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

# Put the libqore directory at the front of LD_LIBRARY_PATH so the build's
# qore binary picks up the build's libqore.so before any installed copy.
# This replaces the old LD_PRELOAD approach which caused "multiple libqore
# images loaded" when an installed copy also existed on the RPATH.
LIBQORE_DIR=$(dirname "$LIBQORE")
export LD_LIBRARY_PATH="${LIBQORE_DIR}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

# Enable core dumps for crash diagnostics
ulimit -c unlimited 2>/dev/null
CORE_ULIMIT=$(ulimit -c 2>/dev/null)
echo "Core dump ulimit: ${CORE_ULIMIT:-unknown}"
# Create a directory for core dumps (used as CI artifact)
# Use a path relative to the project root so GitLab can collect it as an artifact
CORE_DIR="${CORE_DIR:-./crash-dumps}"
mkdir -p "$CORE_DIR" 2>/dev/null
CORE_DIR_ABS=$(cd "$CORE_DIR" && pwd)
# Try to set core pattern to write cores to our directory (may fail in containers)
CORE_PATTERN_SET=0
if [ -w /proc/sys/kernel/core_pattern ]; then
    echo "$CORE_DIR_ABS/core.%e.%p" > /proc/sys/kernel/core_pattern
    CORE_PATTERN_SET=1
fi
# Also try sysctl (works on some systems where /proc write fails)
if [ $CORE_PATTERN_SET -eq 0 ]; then
    sysctl -w "kernel.core_pattern=$CORE_DIR_ABS/core.%e.%p" 2>/dev/null && CORE_PATTERN_SET=1
fi
# Show the actual core pattern so we know where cores will land
if [ -f /proc/sys/kernel/core_pattern ]; then
    KERN_CORE_PATTERN=$(cat /proc/sys/kernel/core_pattern 2>/dev/null)
    echo "kernel.core_pattern: $KERN_CORE_PATTERN"
    if echo "$KERN_CORE_PATTERN" | grep -q '^|'; then
        echo "WARNING: core_pattern pipes to a program; cores may not be saved to disk"
    fi
fi

# Print info about used variables etc.
echo "Using qore: $QORE"
echo "Using libqore: $LIBQORE"
echo "QORE_INCLUDE_DIR=$QORE_INCLUDE_DIR"
echo "QORE_MODULE_DIR=$QORE_MODULE_DIR"
echo "LD_PRELOAD=$LD_PRELOAD"
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
echo "QORE_DB_CONNSTR: ${QORE_DB_CONNSTR}"
echo "QORE_DB_CONNSTR_FREETDS: ${QORE_DB_CONNSTR_FREETDS}"
echo "QORE_DB_CONNSTR_MYSQL: ${QORE_DB_CONNSTR_MYSQL}"
echo "QORE_DB_CONNSTR_PGSQL: ${QORE_DB_CONNSTR_PGSQL}"
echo "QORE_DB_CONNSTR_ORACLE: ${QORE_DB_CONNSTR_ORACLE}"
# Mask credentials in REDIS_URL (replace user:pass@ with ***@)
echo "REDIS_URL: $(echo "${REDIS_URL}" | sed 's|://[^@]*@|://***@|')"

if [ $MEASURE_TIME -eq 1 ]; then
    printf "TIME_CMD: %s\n" "$TIME_CMD"
fi
echo

# Search for tests in the test directory.
TESTS=`find $TEST_DIRS -name "*.qtest" | sort`
FAILED_TESTS=""

# Helper: check if a test matches the perf/stress test pattern
is_perf_test() {
    case "$(basename "$1")" in
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

# Shard tests for parallel CI execution
# GitLab sets CI_NODE_INDEX (1-based) and CI_NODE_TOTAL with parallel: N
if [ -n "$CI_NODE_INDEX" ] && [ -n "$CI_NODE_TOTAL" ] && [ "$CI_NODE_TOTAL" -gt 1 ] 2>/dev/null; then
    SHARDED=""
    j=0
    for test in $TESTS; do
        shard=$(( (j % CI_NODE_TOTAL) + 1 ))
        if [ $shard -eq $CI_NODE_INDEX ]; then
            SHARDED="$SHARDED $test"
        fi
        j=$(( j + 1 ))
    done
    TESTS="$SHARDED"
    if [ $PRINT_TEXT -eq 1 ]; then
        echo "Shard $CI_NODE_INDEX/$CI_NODE_TOTAL selected"
    fi
fi

TEST_COUNT=`echo $TESTS | wc -w`
if [ $TEST_COUNT -eq 0 ]; then
    echo "ERROR: no tests found to run" >&2
    exit 1
fi
PASSED_TEST_COUNT=0
FAILED_TEST_COUNT=0

# Run tests.
i=1
for test in $TESTS; do
    if [ $PRINT_TEXT -eq 1 ]; then
        echo "====================================="
        echo "Running test ($i/$TEST_COUNT): $test"
        echo "-------------------------------------"
        echo "cmdline: $QORE $QORE_TEST_OPTS $test $TEST_OUTPUT_FORMAT"
        echo "-------------------------------------"
    fi

    # Run single test.
    if [ $MEASURE_TIME -eq 1 ]; then
        eval $TIME_CMD $QORE $QORE_TEST_OPTS $test $TEST_OUTPUT_FORMAT
    else
        $QORE $QORE_TEST_OPTS $test $TEST_OUTPUT_FORMAT
    fi

    TEST_EXIT=$?

    if [ $TEST_EXIT -eq 0 ]; then
        PASSED_TEST_COUNT=`expr $PASSED_TEST_COUNT + 1`
    else
        FAILED_TEST_COUNT=`expr $FAILED_TEST_COUNT + 1`
        FAILED_TESTS="$FAILED_TESTS $test"
        # If the test was killed by a signal (exit code > 128), try to capture diagnostics
        if [ $TEST_EXIT -gt 128 ]; then
            SIG_NUM=`expr $TEST_EXIT - 128`
            echo "*** CRASH: test killed by signal $SIG_NUM (exit code $TEST_EXIT) ***"
            # Check for core dump in known locations
            CORE_FILE=""
            for cf in "$CORE_DIR"/core.* "$CORE_DIR_ABS"/core.* core core.* /tmp/core.* \
                       /var/lib/apport/coredump/core.* /var/crash/*.crash; do
                if [ -f "$cf" ] 2>/dev/null; then
                    CORE_FILE="$cf"
                    break
                fi
            done
            TEST_BASENAME=$(basename "$test" .qtest)
            if [ -n "$CORE_FILE" ] && command -v gdb > /dev/null 2>&1; then
                echo "*** Core dump found: $CORE_FILE - extracting backtrace ***"
                BT_FILE="$CORE_DIR/backtrace-${TEST_BASENAME}.txt"
                gdb -batch -ex "thread apply all bt full" -ex "quit" "$QORE" "$CORE_FILE" 2>&1 | tee "$BT_FILE" | head -500
                # Move core to artifact directory for CI collection (don't delete)
                case "$CORE_FILE" in
                    "$CORE_DIR"/*|"$CORE_DIR_ABS"/*) ;;
                    *) cp "$CORE_FILE" "$CORE_DIR/" 2>/dev/null && rm -f "$CORE_FILE" || true ;;
                esac
            elif command -v gdb > /dev/null 2>&1; then
                echo "*** No core dump found; re-running under gdb to try to capture backtrace ***"
                BT_FILE="$CORE_DIR/backtrace-${TEST_BASENAME}.txt"
                # Filter out thread creation/exit noise (both glibc [New Thread] and musl [New LWP] formats)
                gdb -batch -ex run -ex "thread apply all bt full" -ex quit \
                    --args $QORE $QORE_TEST_OPTS $test $TEST_OUTPUT_FORMAT 2>&1 \
                    | grep -v '^\[New Thread\|^\[Thread.*exited\]\|^\[New LWP\|^\[LWP.*exited\]\|^\[Detaching' \
                    | tee "$BT_FILE" | head -500
            else
                echo "*** No core dump found and gdb not available ***"
            fi
        fi
    fi

    i=`expr $i + 1`
    if [ $PRINT_TEXT -eq 1 ]; then echo "-------------------------------------"; echo; fi
done


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

exit $FAILED_TEST_COUNT
