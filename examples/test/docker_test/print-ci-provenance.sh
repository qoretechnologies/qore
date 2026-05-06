#!/bin/sh

# Print CI image/artifact provenance without affecting build or test behavior.

set +e

echo
echo "=== CI provenance ==="
echo "ci_job_name=${CI_JOB_NAME:-unknown}"
echo "ci_runner_description=${CI_RUNNER_DESCRIPTION:-unknown}"
echo "ci_runner_tags=${CI_RUNNER_TAGS:-unknown}"
echo "ci_job_image=${CI_JOB_IMAGE:-unknown}"
echo "ci_commit_sha=${CI_COMMIT_SHA:-unknown}"
echo "qore_src_dir=${QORE_SRC_DIR:-unknown}"
echo "hostname=$(hostname 2>/dev/null || echo unknown)"
echo "uname=$(uname -a 2>/dev/null || echo unknown)"

if [ -f /etc/qore-test-base-build-info ]; then
    echo
    echo "--- qore-test-base build info ---"
    cat /etc/qore-test-base-build-info
else
    echo
    echo "--- qore-test-base build info ---"
    echo "missing: /etc/qore-test-base-build-info"
fi

if command -v qore >/dev/null 2>&1; then
    echo
    echo "--- image qore -V ---"
    qore -V 2>&1 || true
fi

if [ -n "${QORE_SRC_DIR:-}" ] && [ -f "${QORE_SRC_DIR}/build/qore-ci-build-artifact-info" ]; then
    echo
    echo "--- qore build artifact info ---"
    cat "${QORE_SRC_DIR}/build/qore-ci-build-artifact-info"
elif [ -f build/qore-ci-build-artifact-info ]; then
    echo
    echo "--- qore build artifact info ---"
    cat build/qore-ci-build-artifact-info
fi

echo "=== end CI provenance ==="
echo

exit 0
