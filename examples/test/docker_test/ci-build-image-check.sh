#!/bin/sh
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# ci-build-image-check.sh <build artifact info>
#
# Fails a CI test job that runs on another qore-test-base image than the build job whose build artifact it tests.
#
# The build job compiles Qore and the qlib AOT modules against the binary modules of its image (the native modules in
# the image's module directory).  The test jobs of a pipeline run on the image tag that the build job ran on, but the
# tag can be published again between the jobs, as in pipeline 61639, and a test job then tests the build with the
# binary modules of another image.  No test result of such a job is valid, so the job fails before any test runs, and
# the pipeline has to be run again.
#
# The image of the build job is described by the [qore-test-base-build-info] section of the build artifact info that
# test/docker_test/build.sh (or build-perf.sh) writes, and the image of this job by /etc/qore-test-base-build-info, or
# by the file that QORE_TEST_BASE_BUILD_INFO names.  An image is identified by the time it was built (created_utc) and
# the Qore commit it was built from (qore_commit); a job whose image cannot be identified fails as well.
#
# Returns 0 if both jobs ran on the same image; otherwise prints the reason to standard error and returns 1.

info=$1
this_info=${QORE_TEST_BASE_BUILD_INFO:-/etc/qore-test-base-build-info}

# prints the value of a key of the image build information in standard input, or nothing if it has none
image_value() {
    sed -n "s/^$1=//p" | head -n 1
}

fail() {
    echo "$*" >&2
    exit 1
}

if [ -z "${info}" ] || [ ! -f "${info}" ]; then
    fail "the build artifact info '${info}' does not exist: the qore-test-base image of the build job is unknown;" \
        "the build artifact must come from test/docker_test/build.sh"
fi
if [ ! -f "${this_info}" ]; then
    fail "the qore-test-base build information '${this_info}' of this job's image does not exist: the image of this" \
        "job is unknown"
fi

# the section of the image build information in the artifact info, which ends at the next section that the artifact
# info itself adds; the artifact info's own created_utc, before the section, is not the image's
build_section=$(sed -n '/^\[qore-test-base-build-info\]$/,/^\[built-qore-version\]$/p' "${info}")
if [ -z "${build_section}" ]; then
    fail "the build artifact info '${info}' has no [qore-test-base-build-info] section: the qore-test-base image of" \
        "the build job is unknown"
fi
build_created=$(printf '%s\n' "${build_section}" | image_value created_utc)
build_commit=$(printf '%s\n' "${build_section}" | image_value qore_commit)
this_created=$(image_value created_utc < "${this_info}")
this_commit=$(image_value qore_commit < "${this_info}")

if [ -z "${build_created}" ] || [ -z "${build_commit}" ]; then
    fail "the [qore-test-base-build-info] section of the build artifact info '${info}' has no created_utc or" \
        "qore_commit: the qore-test-base image of the build job is unknown"
fi
if [ -z "${this_created}" ] || [ -z "${this_commit}" ]; then
    fail "the qore-test-base build information '${this_info}' has no created_utc or qore_commit: the image of this" \
        "job is unknown"
fi

build_image="${build_created} qore ${build_commit}"
this_image="${this_created} qore ${this_commit}"
if [ "${build_image}" != "${this_image}" ]; then
    fail "qore-test-base image changed during this pipeline (build: ${build_image}, test: ${this_image});" \
        "re-run the pipeline"
fi
exit 0
