#!/bin/sh
# -*- mode: sh -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Sourced by the CI scripts that run test suites (test-ubuntu.sh, test-alpine.sh, test-macos.sh, test-cmake.sh,
# test-perf.sh, test-tsan.sh, check-docs.sh, build-perf.sh, test/cpp/run-sanitizers.sh): a job runs all of its suites,
# also when an earlier one fails, and ends with one summary of every failing suite and of its failing tests, so that a
# failing suite never hides the results of the next (batch rule v9.2 B: in pipeline 70914 test-macos stopped at a
# failure of the runner checks and ran no qtest, which hid a hang until the next pipeline).  A failed build is not a
# suite: it still ends the job, as there is nothing to test.
#
#   qore_ci_suite NAME COMMAND...   runs the command to its end, shows its output as it is written, and records its
#                                   status; it always returns 0, also under set -e
#   qore_ci_suites_summary          prints the summary of the suites run so far; returns 1 if any failed, else 0
#
# The failing tests of a suite are taken from its output: the FAIL: and ERROR: lines of Python's unittest, and the
# lists that run_tests.sh prints at its end ("Failed tests:", "Harness errors: ...").

qore_ci_suites_dir=
qore_ci_suite_count=0

#! qore_ci_suite NAME COMMAND...: runs a suite to its end and records its status for qore_ci_suites_summary
qore_ci_suite() {
    qore_ci_suite_name=$1
    shift
    if [ -z "${qore_ci_suites_dir}" ]; then
        qore_ci_suites_dir=$(mktemp -d "${TMPDIR:-/tmp}/qore-ci-suites.XXXXXX")
    fi
    qore_ci_suite_count=$((qore_ci_suite_count + 1))
    qore_ci_suite_log="${qore_ci_suites_dir}/${qore_ci_suite_count}.log"
    qore_ci_suite_status="${qore_ci_suites_dir}/${qore_ci_suite_count}.status"
    echo "=== suite ${qore_ci_suite_count}: ${qore_ci_suite_name} ==="
    # the group is a subshell of the pipe: set -e of the calling script would end it at a failed command, before its
    # status is written, so it is turned off there
    { set +e; "$@" 2>&1; echo $? > "${qore_ci_suite_status}"; } | tee "${qore_ci_suite_log}"
    qore_ci_suite_rc=$(cat "${qore_ci_suite_status}" 2>/dev/null)
    if [ -z "${qore_ci_suite_rc}" ]; then
        # the status could not be written: the suite did not end normally
        qore_ci_suite_rc=255
    fi
    printf '%s\t%s\n' "${qore_ci_suite_rc}" "${qore_ci_suite_name}" >> "${qore_ci_suites_dir}/results"
    if [ "${qore_ci_suite_rc}" != 0 ]; then
        echo "=== suite ${qore_ci_suite_count}: ${qore_ci_suite_name} FAILED (exit code ${qore_ci_suite_rc}); the job" \
            "runs its other suites and fails at its end ==="
    fi
    return 0
}

#! prints the failing tests of a suite from its log
qore_ci_suite_failing_tests() {
    awk '
        /^(FAIL|ERROR): / { print "    " $0; found = 1; next }
        /^Failed tests:$/ || /^Harness errors: / { in_list = 1; print "    " $0; next }
        in_list && (/^\*+$/ || /^$/ || /^Harness errors: /) { in_list = 0 }
        in_list { print "      " $0; found = 1 }
        END { if (!found) print "    (no failing test named in its output: see the output of the suite above)" }
    ' "$1"
}

#! qore_ci_suites_summary: prints the summary of the suites run so far; returns 1 if any failed, else 0
qore_ci_suites_summary() {
    echo
    echo "=================================== CI suite summary ==================================="
    if [ -z "${qore_ci_suites_dir}" ] || [ ! -f "${qore_ci_suites_dir}/results" ]; then
        echo "no suite was run"
        echo "========================================================================================"
        return 0
    fi
    qore_ci_suites_failed=0
    qore_ci_suite_n=0
    while IFS="$(printf '\t')" read -r qore_ci_suite_rc qore_ci_suite_name; do
        qore_ci_suite_n=$((qore_ci_suite_n + 1))
        if [ "${qore_ci_suite_rc}" = 0 ]; then
            echo "passed: ${qore_ci_suite_name}"
        else
            qore_ci_suites_failed=$((qore_ci_suites_failed + 1))
            echo "FAILED: ${qore_ci_suite_name} (exit code ${qore_ci_suite_rc})"
            qore_ci_suite_failing_tests "${qore_ci_suites_dir}/${qore_ci_suite_n}.log"
        fi
    done < "${qore_ci_suites_dir}/results"
    echo "${qore_ci_suite_n} suites, ${qore_ci_suites_failed} failed"
    echo "========================================================================================"
    rm -rf "${qore_ci_suites_dir}"
    qore_ci_suites_dir=
    qore_ci_suite_count=0
    [ "${qore_ci_suites_failed}" = 0 ]
}
