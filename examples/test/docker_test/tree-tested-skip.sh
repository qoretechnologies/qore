#!/bin/sh
# -*- mode: sh -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Sourced first by every job of the pipeline (before anything is built, tested or reported): when the
# tree-already-tested job found that this commit's tree already passed CI (QORE_TREE_TESTED_IN, see
# tree-already-tested.sh), the job ends at once with success.  Otherwise nothing happens.  It is sourced, so its
# "exit" ends the job's script.  It posts no GitHub status: when every job has passed, pipeline-passed and
# github-status post the required status, pointing to the pipeline that passed the tree (ci-github-status.sh).

if [ -n "${QORE_TREE_TESTED_IN:-}" ]; then
    echo "this tree already passed in pipeline ${QORE_TREE_TESTED_IN} (commit ${QORE_TREE_TESTED_SHA:-?}):"
    echo "  ${CI_PROJECT_URL}/-/pipelines/${QORE_TREE_TESTED_IN}"
    echo "nothing to do"
    exit 0
fi
