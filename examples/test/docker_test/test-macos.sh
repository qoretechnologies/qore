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
    -DCMAKE_BUILD_TYPE=release \
    -DSINGLE_COMPILATION_UNIT=1

# Build
make -j${MAKE_JOBS}

echo "=== Running Tests ==="
cd "${QORE_SRC_DIR}"

# Set module path to include built modules
export QORE_MODULE_DIR="${QORE_SRC_DIR}/qlib:${QORE_MODULE_DIR:-}"

# Run tests using the built qore binary
export PATH="${BUILD_DIR}:${PATH}"

# Skip max thread test on macOS - it can cause kernel panics on Apple Silicon
# when creating 8000+ threads overwhelms the pthread subsystem
export SKIP_MAX_THREAD_TEST=1

# Run the test suite (QORE_TEST_OPTS is read from the environment by run_tests.sh)
./run_tests.sh

echo "=== macOS CI Tests Complete ==="
