#!/bin/sh
# -*- mode: sh -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Sourced first by every job of the pipeline (before anything is built, tested or reported): when the
# tree-already-tested job found that this commit's tree already passed CI (QORE_TREE_TESTED_IN, see
# tree-already-tested.sh), the job reports success to GitHub, pointing to the pipeline that passed the tree, and ends
# at once.  Otherwise nothing happens.  It is sourced, so its "exit" ends the job's script.

if [ -n "${QORE_TREE_TESTED_IN:-}" ]; then
    tested_url="${CI_PROJECT_URL}/-/pipelines/${QORE_TREE_TESTED_IN}"
    echo "this tree already passed in pipeline ${QORE_TREE_TESTED_IN} (commit ${QORE_TREE_TESTED_SHA:-?}):"
    echo "  ${tested_url}"
    echo "nothing to do"
    if [ -n "${GITHUB_ACCESS_TOKEN:-}" ]; then
        set +x
        curl -s --max-time 30 "https://api.github.com/repos/qoretechnologies/${REPO_NAME}/statuses/${CI_COMMIT_SHA}" \
            -X POST --oauth2-bearer "${GITHUB_ACCESS_TOKEN}" -H "Content-Type: application/json" \
            -d "{\"state\": \"success\", \"context\": \"${REPO_NAME}\", \"description\": \"tree tested in pipeline ${QORE_TREE_TESTED_IN}\", \"target_url\": \"${tested_url}\"}" \
            > /dev/null || true
    fi
    exit 0
fi
