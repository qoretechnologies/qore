#!/bin/bash

set -e
set -x

# macOS CI test script for GitLab Runner (Tart VM executor)

# Setup Homebrew environment (Apple Silicon)
if [ -x /opt/homebrew/bin/brew ]; then
    eval "$(/opt/homebrew/bin/brew shellenv)"
    HB="$(brew --prefix)"
    # Add keg-only package paths so CMake/find_program can locate them
    export PATH="${HB}/opt/bison/bin:${HB}/opt/flex/bin:${PATH}"
    # Ensure linker and pkg-config find Homebrew libraries (including keg-only)
    KEG_PKGS=(openssl@3 libxml2 libxslt zlib bzip2 expat readline ncurses libffi sqlite openldap krb5 libarchive curl libtool)
    for pkg in "${KEG_PKGS[@]}"; do
        kp="$(brew --prefix "$pkg" 2>/dev/null)" || continue
        [ -d "$kp/lib/pkgconfig" ] && export PKG_CONFIG_PATH="$kp/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
        [ -d "$kp/lib" ] && export LDFLAGS="-L$kp/lib ${LDFLAGS:-}"
        [ -d "$kp/include" ] && export CPPFLAGS="-I$kp/include ${CPPFLAGS:-}"
    done
    export LDFLAGS="-L${HB}/lib ${LDFLAGS:-}"
    export CPPFLAGS="-I${HB}/include ${CPPFLAGS:-}"
    export PKG_CONFIG_PATH="${HB}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
fi

# Add cargo bin to PATH (tree-sitter CLI installed via cargo)
if [ -d "$HOME/.cargo/bin" ]; then
    export PATH="$HOME/.cargo/bin:${PATH}"
fi

# Setup source directory
QORE_SRC_DIR="${QORE_SRC_DIR:-$(pwd)}"
cd "${QORE_SRC_DIR}"

# Number of parallel jobs (use sysctl on macOS)
MAKE_JOBS="${MAKE_JOBS:-$(sysctl -n hw.ncpu)}"

# Build directory - use a clean build each time
BUILD_DIR="${QORE_SRC_DIR}/build"
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

# install tree-sitter CLI for astparser module build
if ! command -v tree-sitter > /dev/null 2>&1; then
    echo "=== Installing tree-sitter CLI ==="
    cargo install tree-sitter-cli@0.26.5
fi

echo "=== Building Qore on macOS ==="
cd "${BUILD_DIR}"

# Configure with CMake
# Detect package manager prefix: Homebrew (/opt/homebrew) or MacPorts (/opt/local)
if [ -d /opt/homebrew ]; then
    # Homebrew on Apple Silicon; include keg-only package prefixes for CMake
    CMAKE_PREFIX="/opt/homebrew;/opt/homebrew/opt/openssl@3;/opt/homebrew/opt/libxml2"
elif [ -d /opt/local ]; then
    # MacPorts
    CMAKE_PREFIX="/opt/local"
else
    CMAKE_PREFIX=""
fi

cmake .. \
    -DCMAKE_BUILD_TYPE=release \
    -DSINGLE_COMPILATION_UNIT=1 \
    ${CMAKE_PREFIX:+-DCMAKE_PREFIX_PATH="${CMAKE_PREFIX}"}

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
