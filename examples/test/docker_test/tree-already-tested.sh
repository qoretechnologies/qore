#!/bin/sh
# -*- mode: sh -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Run by the tree-already-tested job of a develop pipeline: decides whether the commit's tree already passed CI, so
# that the pipeline does not test the same code twice.  Merging a pull request that is up to date with develop makes
# a merge commit whose tree is the pull request head's tree, which the pull request pipeline has just tested (pipeline
# 63069 on 4672068e43 tested the tree of 08cc1e0cb7, which pipeline 63064 had passed: 28 jobs, 16 runner-hours).
#
# The candidates are the commit itself (a fast-forward merge, or a pipeline on another ref) and each parent with the
# same tree (the merged pull request head).  The first candidate with a successful pipeline other than this one is
# written to tree-tested.env (a dotenv report):
#   QORE_TREE_TESTED_IN    the ID of the pipeline that passed the tree
#   QORE_TREE_TESTED_SHA   its commit
# and every other job of the pipeline ends at once with success (tree-tested-skip.sh).
#
# It fails open: when the tree has no successful pipeline, or the pipelines cannot be read (no token, an API error),
# tree-tested.env names no pipeline and the whole pipeline runs, as without this job.  The pipelines are read with the
# job token, or with QORE_CI_READ_API_TOKEN (a read_api token, a masked CI variable) where the job token is refused.

set -u
out=tree-tested.env
# a dotenv report needs one variable even when the tree must be tested
echo "QORE_TREE_TESTED_CHECKED=1" > "$out"

tree=$(git rev-parse 'HEAD^{tree}') || { echo "cannot read the commit's tree: running the full pipeline"; exit 0; }
candidates=$(git rev-parse HEAD)
for parent in $(git rev-list --parents -n 1 HEAD | cut -d' ' -f2-); do
    if [ "$(git rev-parse "${parent}^{tree}" 2>/dev/null)" = "$tree" ]; then
        candidates="$candidates $parent"
    fi
done

# never trace a token
set +x
pipelines() {
    url="${CI_API_V4_URL}/projects/${CI_PROJECT_ID}/pipelines?sha=$1&status=success&per_page=20"
    if [ -n "${CI_JOB_TOKEN:-}" ] && curl -sf --max-time 30 --header "JOB-TOKEN: ${CI_JOB_TOKEN}" "$url"; then
        return 0
    fi
    if [ -n "${QORE_CI_READ_API_TOKEN:-}" ] \
        && curl -sf --max-time 30 --header "PRIVATE-TOKEN: ${QORE_CI_READ_API_TOKEN}" "$url"; then
        return 0
    fi
    return 1
}

for commit in $candidates; do
    if ! json=$(pipelines "$commit"); then
        echo "cannot read the pipelines of $commit: running the full pipeline"
        exit 0
    fi
    # a pipeline list item: {"id":63064,"iid":...,"project_id":...,"sha":"...","status":"success",...}
    for id in $(printf '%s' "$json" | grep -oE '"id":[0-9]+' | cut -d: -f2); do
        if [ "$id" = "${CI_PIPELINE_ID:-}" ]; then
            continue
        fi
        printf 'QORE_TREE_TESTED_IN=%s\nQORE_TREE_TESTED_SHA=%s\n' "$id" "$commit" >> "$out"
        echo "tree $tree already passed in pipeline $id on commit $commit: the jobs of this pipeline end at once"
        exit 0
    done
done
echo "tree $tree has no successful pipeline: running the full pipeline"
