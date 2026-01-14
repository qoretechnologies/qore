#!/bin/bash

set -e
set -x

# macOS CI test script for GitLab Runner (shell executor)

# Setup source directory
QORE_SRC_DIR="${QORE_SRC_DIR:-$(pwd)}"
cd "${QORE_SRC_DIR}"

# Number of parallel jobs (use sysctl on macOS)
MAKE_JOBS="${MAKE_JOBS:-$(sysctl -n hw.ncpu)}"

# Build directory - use a clean build each time
BUILD_DIR="${QORE_SRC_DIR}/build-ci"
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

echo "=== Building Qore on macOS ==="
cd "${BUILD_DIR}"

# Configure with CMake
cmake .. \
    -DCMAKE_BUILD_TYPE=debug \
    -DSINGLE_COMPILATION_UNIT=1

# Build
make -j${MAKE_JOBS}

echo "=== Running Tests ==="
cd "${QORE_SRC_DIR}"

# Set module path to include built modules
export QORE_MODULE_DIR="${QORE_SRC_DIR}/qlib:${QORE_MODULE_DIR:-}"

# Run tests using the built qore binary
export PATH="${BUILD_DIR}:${PATH}"

# Run the test suite
./run_tests.sh ${QORE_TEST_OPTS:-}

echo "=== macOS CI Tests Complete ==="
