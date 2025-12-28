#!/bin/sh

print_usage () {
  echo "Usage: run_tests.sh [OPTIONS]"
  echo "Run qore tests."
  echo
  echo "  -d <dir>   Run only specified tests (as found in $BASE_TEST_PATH)."
  echo "  -j         Use --format=junit option for the tests, making them print JUnit output."
  echo "  -t         Measure execution time of the tests."
  echo "  -v         Use --format=plain option for the tests, making them print one statement per each test case."
  echo
  echo "Environment variables:"
  echo "  QORE_TEST_OPTS   Additional options to pass to qore (e.g., '-penable-debug')."
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

while getopts ":d:jvt" opt; do
    case $opt in
        d)
            TEST_DIRS="$TEST_DIRS \"$BASE_TEST_PATH/$OPTARG\""
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

# If no test dirs were specified, run all the tests
if [ -z "$TEST_DIRS" ]; then
    TEST_DIRS=$BASE_TEST_PATH
fi

QORE=""
QR=""
LIBQORE=""
QORE_LIB_PATH="./lib/.libs:./qlib:$LD_LIBRARY_PATH"

# Test that qore is built.
if [ -s "./.libs/qore" ] && [ -f "./qore" ] && [ -f "./lib/.libs/libqore.so" -o "./lib/.libs/libqore.dylib" ]; then
    if [ -f "./lib/.libs/libqore.so" ]; then
        LIBQORE="./lib/.libs/libqore.so"
    elif [ -f "./lib/.libs/libqore.dylib" ]; then
        LIBQORE="./lib/.libs/libqore.dylib"
    fi
    QORE="./.libs/qore"
    QR="./.libs/qr"
else
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

if [ -z "$QORE" ] || [ -z "$LIBQORE" ]; then
    echo "Qore is not built. Exiting."
    exit 1
fi

export LD_LIBRARY_PATH=$QORE_LIB_PATH
export QORE_MODULE_DIR=./qlib:$QORE_MODULE_DIR

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

LD_PRELOAD=$LIBQORE

# Print info about used variables etc.
echo "Using qore: $QORE"
echo "Using libqore: $LIBQORE"
echo "QORE_INCLUDE_DIR=$QORE_INCLUDE_DIR"
echo "QORE_MODULE_DIR=$QORE_MODULE_DIR"
echo "LD_PRELOAD=$LD_PRELOAD"
export LD_LIBRARY_PATH=$LD_PRELOAD:$LD_LIBRARY_PATH
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
TESTS=`eval find "$TEST_DIRS" -name "*.qtest"`
FAILED_TESTS=""

TEST_COUNT=`echo $TESTS | wc -w`
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

    if [ $? -eq 0 ]; then
        PASSED_TEST_COUNT=`expr $PASSED_TEST_COUNT + 1`
    else
        FAILED_TEST_COUNT=`expr $FAILED_TEST_COUNT + 1`
        FAILED_TESTS="$FAILED_TESTS $test"
    fi

    i=`expr $i + 1`
    if [ $PRINT_TEXT -eq 1 ]; then echo "-------------------------------------"; echo; fi
done

unset LD_PRELOAD

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
