# -*- mode: sh -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Sourced by the CI scripts: authenticates git's HTTPS requests to github.com with GITHUB_ACCESS_TOKEN (a CI
# variable of the GitLab mirror group), so that the clones CMake makes at configure time (FetchContent, for example
# the tree-sitter runtime of the astparser module) are not refused as anonymous requests: GitHub answered those with
# an authentication challenge in pipeline 57935, which git reports as "could not read Username".
#
# The token is passed to git in environment-only configuration (GIT_CONFIG_COUNT / GIT_CONFIG_KEY_<n> /
# GIT_CONFIG_VALUE_<n>, git 2.31 or later) as an Authorization header, so it appears neither in a clone URL, which
# CMake prints when a clone fails, nor in the CMake cache or any file.  Shell tracing is suspended while the token
# is handled, and restored afterwards.  Without GITHUB_ACCESS_TOKEN (local runs), nothing is changed.

# tracing is suspended before the token is first expanded, even in the test for it
case $- in
    *x*) _qore_gh_xtrace=1; { set +x; } 2>/dev/null ;;
    *) _qore_gh_xtrace= ;;
esac
if [ -n "${GITHUB_ACCESS_TOKEN}" ]; then
    _qore_gh_idx=${GIT_CONFIG_COUNT:-0}
    _qore_gh_auth=$(printf '%s' "x-access-token:${GITHUB_ACCESS_TOKEN}" | base64 | tr -d '\n')
    export GIT_CONFIG_COUNT=$((_qore_gh_idx + 1))
    export "GIT_CONFIG_KEY_${_qore_gh_idx}=http.https://github.com/.extraheader"
    export "GIT_CONFIG_VALUE_${_qore_gh_idx}=AUTHORIZATION: basic ${_qore_gh_auth}"
    unset _qore_gh_auth _qore_gh_idx
    echo "-- git requests to github.com are authenticated with GITHUB_ACCESS_TOKEN --"
fi
if [ -n "${_qore_gh_xtrace}" ]; then
    unset _qore_gh_xtrace
    set -x
fi
