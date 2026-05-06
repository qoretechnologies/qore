#!/bin/bash

set -e
set -x

# macOS CI test script for GitLab Runner.
#
# Package manager preference: MacPorts (/opt/local) is preferred and used by
# the prof CI runner.  Homebrew (/opt/homebrew) is supported as a fallback for
# ad-hoc local runs only.  We detect MacPorts via /opt/local/bin/port (not
# just directory presence) so a stray /opt/homebrew tree on a MacPorts host
# does not flip the script into Homebrew mode.

PM=
if [ -x /opt/local/bin/port ]; then
    PM=macports
elif [ -x /opt/homebrew/bin/brew ]; then
    PM=homebrew
fi

if [ "$PM" = "macports" ]; then
    export PATH="/opt/local/bin:/opt/local/sbin:${PATH}"
    export PKG_CONFIG_PATH="/opt/local/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
    export LDFLAGS="-L/opt/local/lib ${LDFLAGS:-}"
    export CPPFLAGS="-I/opt/local/include ${CPPFLAGS:-}"
elif [ "$PM" = "homebrew" ]; then
    eval "$(/opt/homebrew/bin/brew shellenv)"
    HB="$(brew --prefix)"
    # Add keg-only package paths so CMake/find_program can locate them
    export PATH="${HB}/opt/bison/bin:${HB}/opt/flex/bin:${PATH}"
    # Ensure linker and pkg-config find Homebrew libraries (including keg-only)
    KEG_PKGS=(openssl@3 libxml2 libxslt zlib bzip2 expat readline ncurses libffi sqlite openldap krb5 c-ares libarchive curl libtool ossp-uuid libyaml)
    for pkg in "${KEG_PKGS[@]}"; do
        kp="$(brew --prefix "$pkg" 2>/dev/null)" || continue
        [ -d "$kp/lib/pkgconfig" ] && export PKG_CONFIG_PATH="$kp/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
        [ -d "$kp/lib" ] && export LDFLAGS="-L$kp/lib ${LDFLAGS:-}"
        [ -d "$kp/include" ] && export CPPFLAGS="-I$kp/include ${CPPFLAGS:-}"
    done
    export LDFLAGS="-L${HB}/lib ${LDFLAGS:-}"
    export CPPFLAGS="-I${HB}/include ${CPPFLAGS:-}"
    export PKG_CONFIG_PATH="${HB}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

    # Self-heal: install build-critical Homebrew packages that may be
    # missing from a stale Tart VM image (image was built before the
    # package was added to qore-test-base/prep-macos.sh).  CMakeLists
    # requires Eigen3 unconditionally for the ml/kalman modules; the
    # build aborts at find_package(Eigen3 REQUIRED) if it isn't
    # present.  brew install is idempotent (no-op if already installed)
    # so this is safe to run on a fresh image too.
    REQUIRED_BREW_PKGS=(eigen krb5 c-ares)
    for pkg in "${REQUIRED_BREW_PKGS[@]}"; do
        if ! brew list "$pkg" >/dev/null 2>&1; then
            echo "=== Installing missing Homebrew package: $pkg ==="
            brew install "$pkg"
        fi
    done
fi

# Add cargo bin to PATH (tree-sitter CLI installed via cargo)
if [ -d "$HOME/.cargo/bin" ]; then
    export PATH="$HOME/.cargo/bin:${PATH}"
fi

# Setup source directory
QORE_SRC_DIR="${QORE_SRC_DIR:-$(pwd)}"
export QORE_SRC_DIR
cd "${QORE_SRC_DIR}"

./test/docker_test/print-ci-provenance.sh || true

# Number of parallel jobs (use sysctl on macOS)
MAKE_JOBS="${MAKE_JOBS:-$(sysctl -n hw.ncpu)}"

# Build directory - use a clean build each time
BUILD_DIR="${QORE_SRC_DIR}/build"
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

# install Rust/cargo if not available (needed for tree-sitter-cli)
if ! command -v cargo > /dev/null 2>&1; then
    echo "=== Installing Rust toolchain ==="
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --default-toolchain stable --profile minimal
    source "$HOME/.cargo/env"
fi

# install tree-sitter CLI for astparser module build
if ! command -v tree-sitter > /dev/null 2>&1; then
    echo "=== Installing tree-sitter CLI ==="
    cargo install tree-sitter-cli@0.26.5
fi

echo "=== Building Qore on macOS ==="
cd "${BUILD_DIR}"

# Find LLVM installation (Homebrew, MacPorts, manual install, or llvm-config)
LLVM_PREFIX=""
for dir in /opt/homebrew/opt/llvm /usr/local/opt/llvm /opt/homebrew/opt/llvm@* /usr/local/opt/llvm@*; do
    if [ -d "$dir" ]; then
        LLVM_PREFIX="$dir"
    fi
done
# Check MacPorts (highest version)
if [ -z "$LLVM_PREFIX" ]; then
    for dir in /opt/local/libexec/llvm-*; do
        if [ -d "$dir" ]; then
            LLVM_PREFIX="$dir"
        fi
    done
fi
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

# Configure with CMake using the package-manager prefix selected above
case "$PM" in
    macports)
        CMAKE_PREFIX="/opt/local"
        PM_PREFIX="/opt/local"
        ;;
    homebrew)
        # Homebrew on Apple Silicon; include keg-only package prefixes for CMake
        CMAKE_PREFIX="/opt/homebrew;/opt/homebrew/opt/openssl@3;/opt/homebrew/opt/libxml2;/opt/homebrew/opt/ossp-uuid"
        PM_PREFIX="${HB:-/opt/homebrew}"
        ;;
    *)
        CMAKE_PREFIX=""
        PM_PREFIX=""
        ;;
esac

BASE_QORE_MODULE_DIR="${QORE_MODULE_DIR:-}"
EXTRA_MODULE_DIRS=""

add_macos_module_dir() {
    [ -d "$1" ] && EXTRA_MODULE_DIRS="$1:${EXTRA_MODULE_DIRS}"
    return 0
}

add_macos_module_dirs() {
    qore_api_ver="$1"
    for p in "${PM_PREFIX}" /usr/local; do
        [ -z "$p" ] && continue
        if [ -n "$qore_api_ver" ]; then
            add_macos_module_dir "$p/lib/qore-modules/${qore_api_ver}"
            add_macos_module_dir "$p/share/qore-modules/${qore_api_ver}"
        fi
        add_macos_module_dir "$p/lib/qore-modules"
        add_macos_module_dir "$p/share/qore-modules"
    done
}

# AOT qlib compilation happens during `make` and can require pre-installed
# binary modules such as json/yaml/uuid, so expose their unversioned paths now.
add_macos_module_dirs ""
export QORE_MODULE_DIR="${EXTRA_MODULE_DIRS}${BASE_QORE_MODULE_DIR}"

CMAKE_PREFIX_PATH="${CMAKE_PREFIX}"
if [ -n "$LLVM_PREFIX" ]; then
    echo "Found LLVM at: $LLVM_PREFIX"
    if [ -n "$CMAKE_PREFIX_PATH" ]; then
        CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH};${LLVM_PREFIX}"
    else
        CMAKE_PREFIX_PATH="${LLVM_PREFIX}"
    fi
fi

# Configure with CMake
# CMAKE_PREFIX_PATH includes package-manager dependencies and LLVM prefix if found
cmake .. \
    -DCMAKE_BUILD_TYPE=release \
    -DSINGLE_COMPILATION_UNIT=1 \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}"

# Build
make -j${MAKE_JOBS}

echo "=== Running Tests ==="
cd "${QORE_SRC_DIR}"

# Set module path to include built modules and pre-installed binary modules.
# Binary modules (json, yaml, uuid, etc.) may live under the package manager's
# prefix or under /usr/local — qore-test-base installs them to /usr/local
# (INSTALL_PREFIX=/usr/local in prep-macos.sh).  Probe both.
EXTRA_MODULE_DIRS=""
QORE_API_VER=$(build/qore --module-api 2>/dev/null || true)
add_macos_module_dirs "$QORE_API_VER"
export QORE_MODULE_DIR="${QORE_SRC_DIR}/qlib:${EXTRA_MODULE_DIRS}${BASE_QORE_MODULE_DIR}"

# Run tests using the built qore binary
export PATH="${BUILD_DIR}:${PATH}"

# Skip max thread test on macOS - it can cause kernel panics on Apple Silicon
# when creating 8000+ threads overwhelms the pthread subsystem
export SKIP_MAX_THREAD_TEST=1

# Increase timeout for macOS - WebSocketH2PerfTest is slow with -penable-debug flag
# even in Release mode on macOS CI. Default 300s is not enough.
export TEST_TIMEOUT=600

# Raise the file descriptor limit — macOS defaults to 256 which is too low for
# tests that create many sockets (e.g. AsyncSocketIo scalability test needs ~430)
ulimit -n 4096

# Run the test suite (QORE_TEST_OPTS is read from the environment by run_tests.sh)
./run_tests.sh

echo "=== macOS CI Tests Complete ==="
