#!/bin/sh
# -*- mode: sh -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Posts the required GitHub commit status of a pipeline (the context "${REPO_NAME}"), which no other job posts
# (qore#7203: GitHub keeps the last state of a context, so when every test job posted its own result there, the check
# showed the job that finished last, and a job that passed after another had failed made it green).
#
#   ci-github-status.sh pending   at the start of the pipeline (resolve-qore-test-base-images): the check exists
#                                 from then on; one short attempt, which never fails or delays the job much
#   ci-github-status.sh passed    pipeline-passed, which needs every other job of the pipeline and runs only when all
#                                 of them passed: writes the marker file (its artifact, uploaded in every outcome)
#                                 and posts success; fails the job when the status cannot be posted, as after a
#                                 retried job github-status, which has run, does not post it again
#   ci-github-status.sh final     github-status, which needs pipeline-passed and runs in every outcome: posts
#                                 success when pipeline-passed left its marker, and failure otherwise (a job failed,
#                                 was canceled, or was skipped as a job it needs did not pass); fails the job when
#                                 the status cannot be posted
#
# A pipeline whose tree already passed (tree-tested-skip.sh: QORE_TREE_TESTED_IN) posts success pointing to the
# pipeline that passed the tree.  QORE_CI_GITHUB_STATUS_DRY_RUN=1 prints the request body instead of posting it
# (examples/test/runner/test_ci_required_status.py).  The token is never printed.

set -u

marker="${QORE_CI_PASSED_MARKER:-ci-pipeline-passed}"
pipeline_url="${CI_PIPELINE_URL:-}"

post() {
    state=$1
    description=$2
    target_url=$3
    # pending: one attempt of 10 s at most; success and failure: up to 4 attempts of 30 s
    if [ "$state" = pending ]; then
        attempts="--max-time 10"
    else
        attempts="--max-time 30 --retry 3 --retry-delay 5"
    fi
    body=$(printf '{"state": "%s", "context": "%s", "description": "%s", "target_url": "%s"}' \
        "$state" "${REPO_NAME}" "$description" "$target_url")
    if [ -n "${QORE_CI_GITHUB_STATUS_DRY_RUN:-}" ]; then
        printf '%s\n' "$body"
        return 0
    fi
    if [ -z "${GITHUB_ACCESS_TOKEN:-}" ]; then
        echo "GITHUB_ACCESS_TOKEN is not set: the status \"$state\" of ${REPO_NAME} was not posted" >&2
        return 1
    fi
    # never trace the token
    set +x
    if ! curl -sS -f -o /dev/null $attempts \
        "https://api.github.com/repos/qoretechnologies/${REPO_NAME}/statuses/${CI_COMMIT_SHA}" \
        -X POST --oauth2-bearer "${GITHUB_ACCESS_TOKEN}" -H "Content-Type: application/json" -d "$body"; then
        echo "the status \"$state\" of ${REPO_NAME} could not be posted" >&2
        return 1
    fi
    echo "posted \"$state\" to the GitHub status ${REPO_NAME} of ${CI_COMMIT_SHA}: $description"
}

success() {
    if [ -n "${QORE_TREE_TESTED_IN:-}" ]; then
        post success "tree tested in pipeline ${QORE_TREE_TESTED_IN}" \
            "${CI_PROJECT_URL:-}/-/pipelines/${QORE_TREE_TESTED_IN}"
    else
        post success "Gitlab CI: every job passed" "$pipeline_url"
    fi
}

case "${1:-}" in
    pending)
        post pending "Gitlab CI" "$pipeline_url" || true
        ;;
    passed)
        : > "$marker"
        success
        ;;
    final)
        if [ -f "$marker" ]; then
            success
        else
            echo "pipeline-passed did not pass (a job failed, was canceled or was skipped): posting failure"
            post failure "Gitlab CI: a job failed or was canceled" "$pipeline_url"
        fi
        ;;
    *)
        echo "usage: $0 pending|passed|final" >&2
        exit 2
        ;;
esac
