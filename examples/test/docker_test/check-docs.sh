#!/bin/sh
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# The checks of the release notes and module main pages that the check-module-release-notes CI job runs: each runs,
# also when an earlier one fails, and the job ends with the summary of the failing ones (see ci-suites.sh; batch rule
# v9.2 B).  Run from the source directory.

set -e

QORE_SRC_DIR=${QORE_SRC_DIR:-$(pwd)}
. "${QORE_SRC_DIR}/test/docker_test/ci-suites.sh"

#! qore_docs_check_files CHECKER PATTERN...: runs the checker with the tracked files that the patterns match
#  With no such file, the checker is not run, on every platform (GNU xargs would run it once with no file, BSD xargs,
#  as on macOS, would not), and the suite says so; a failure of git ls-files fails the suite.
qore_docs_check_files() {
    qore_docs_checker=$1
    shift
    qore_docs_files=$(git ls-files -- "$@") || return 1
    if [ -z "${qore_docs_files}" ]; then
        echo "no tracked file matches $*: nothing to check"
        return 0
    fi
    git ls-files -z -- "$@" | xargs -0 ${qore_docs_checker}
}

qore_ci_suite "module release notes tests" python3 examples/test/cmake/test_module_release_notes.py
qore_ci_suite "module release notes" qore_docs_check_files "python3 -I doxygen/check-module-release-notes.py" \
    'qlib/*.qm' 'qlib/**/*.qm' 'modules/*/docs/*.dox.tmpl'
qore_ci_suite "release notes tests" python3 examples/test/cmake/test_release_notes.py
qore_ci_suite "release notes" python3 -I doxygen/check-release-notes.py
qore_ci_suite "module main page layout tests" python3 examples/test/cmake/test_module_mainpage_layout.py
qore_ci_suite "module main pages" qore_docs_check_files "python3 -I doxygen/check-module-mainpage.py --sources" \
    'qlib/*.qm' 'qlib/**/*.qm' 'qlib/**/*.qc' 'modules/*/docs/*.dox.tmpl'
qore_ci_suites_summary
