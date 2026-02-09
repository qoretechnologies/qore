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

# Find LLVM installation (Homebrew, manual install, or llvm-config)
LLVM_PREFIX=""
for dir in /opt/homebrew/opt/llvm /usr/local/opt/llvm /opt/homebrew/opt/llvm@* /usr/local/opt/llvm@*; do
    if [ -d "$dir" ]; then
        LLVM_PREFIX="$dir"
    fi
done
# Check for manually installed LLVM (e.g., /usr/local/Cellar/llvm/*)
if [ -z "$LLVM_PREFIX" ]; then
    for dir in /usr/local/Cellar/llvm/*/lib/cmake/llvm; do
        if [ -d "$dir" ]; then
            LLVM_PREFIX="$(dirname "$(dirname "$(dirname "$dir")")")"
        fi
    done
fi
# Try llvm-config as last resort (works for manual/source installs)
if [ -z "$LLVM_PREFIX" ]; then
    if command -v llvm-config >/dev/null 2>&1; then
        LLVM_PREFIX="$(llvm-config --prefix)"
    fi
fi

CMAKE_EXTRA_ARGS=""
if [ -n "$LLVM_PREFIX" ]; then
    echo "Found LLVM at: $LLVM_PREFIX"
    CMAKE_EXTRA_ARGS="-DCMAKE_PREFIX_PATH=${LLVM_PREFIX}"
fi

# Configure with CMake
cmake .. \
    -DCMAKE_BUILD_TYPE=release \
    -DSINGLE_COMPILATION_UNIT=1 \
    ${CMAKE_EXTRA_ARGS}

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
