#!/bin/sh
# -*- mode: sh -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Sourced by the CI scripts that build Qore (build.sh, test-macos.sh, build-separate-units.sh, build-perf.sh): keeps the
# compiler output of a build in a log for the check-build-warnings job (check-build-warnings.py, qore#7757).

#! qore_ci_build_logged LOG COMMAND...: runs the command, shows its output as it is written and keeps it in LOG
#  The first line of the log names the source directory of the build (QORE_SRC_DIR), so that the checker finds the
#  files of the tree in it however the runner names its build directory.  The status is the command's: sh has no
#  pipefail, so it goes around the pipe in a file.
qore_ci_build_logged() {
    qore_ci_build_log=$1
    shift
    qore_ci_build_status=$(mktemp)
    printf 'qore-ci-build-log source-dir=%s\n' "${QORE_SRC_DIR}" > "${qore_ci_build_log}"
    # the group is a subshell of the pipe: set -e of the calling script (build.sh) would end it at a failed command,
    # before its status is written, so it is turned off there
    { set +e; "$@" 2>&1; echo $? > "${qore_ci_build_status}"; } | tee -a "${qore_ci_build_log}"
    qore_ci_build_rc=$(cat "${qore_ci_build_status}")
    rm -f "${qore_ci_build_status}"
    if [ "${qore_ci_build_rc}" != 0 ]; then
        echo "the build failed (exit code ${qore_ci_build_rc}): $*"
    fi
    return "${qore_ci_build_rc}"
}
