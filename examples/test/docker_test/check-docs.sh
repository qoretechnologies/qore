#!/bin/sh
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# The checks of the release notes and module main pages that the check-module-release-notes CI job runs: each runs,
# also when an earlier one fails, and the job ends with the summary of the failing ones (see ci-suites.sh; batch rule
# v9.2 B).  Run from the source directory.

set -e

QORE_SRC_DIR=${QORE_SRC_DIR:-$(pwd)}
. "${QORE_SRC_DIR}/test/docker_test/ci-suites.sh"

qore_ci_suite "module release notes tests" python3 examples/test/cmake/test_module_release_notes.py
qore_ci_suite "module release notes" sh -c "git ls-files -z 'qlib/*.qm' 'qlib/**/*.qm' 'modules/*/docs/*.dox.tmpl' \
    | xargs -0 python3 -I doxygen/check-module-release-notes.py"
qore_ci_suite "release notes tests" python3 examples/test/cmake/test_release_notes.py
qore_ci_suite "release notes" python3 -I doxygen/check-release-notes.py
qore_ci_suite "module main page layout tests" python3 examples/test/cmake/test_module_mainpage_layout.py
qore_ci_suite "module main pages" sh -c "git ls-files -z 'qlib/*.qm' 'qlib/**/*.qm' 'qlib/**/*.qc' \
    'modules/*/docs/*.dox.tmpl' | xargs -0 python3 -I doxygen/check-module-mainpage.py --sources"
qore_ci_suites_summary
