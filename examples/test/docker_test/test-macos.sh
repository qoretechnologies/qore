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
cd "${QORE_SRC_DIR}"

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

# Configure with CMake using the package-manager prefix selected above
case "$PM" in
    macports)
        CMAKE_PREFIX="/opt/local"
        ;;
    homebrew)
        # Homebrew on Apple Silicon; include keg-only package prefixes for CMake
        CMAKE_PREFIX="/opt/homebrew;/opt/homebrew/opt/openssl@3;/opt/homebrew/opt/libxml2;/opt/homebrew/opt/ossp-uuid"
        ;;
    *)
        CMAKE_PREFIX=""
        ;;
esac

cmake .. \
    -DCMAKE_BUILD_TYPE=release \
    -DSINGLE_COMPILATION_UNIT=1 \
    ${CMAKE_PREFIX:+-DCMAKE_PREFIX_PATH="${CMAKE_PREFIX}"}

# Build
make -j${MAKE_JOBS}

echo "=== Running Tests ==="
cd "${QORE_SRC_DIR}"

# Set module path to include built modules and pre-installed binary modules.
# Binary modules (json, yaml, uuid, etc.) may live under the package manager's
# prefix or under /usr/local — qore-test-base installs them to /usr/local
# (INSTALL_PREFIX=/usr/local in prep-macos.sh).  Probe both.
EXTRA_MODULE_DIRS=""
case "$PM" in
    macports) PM_PREFIX="/opt/local" ;;
    homebrew) PM_PREFIX="${HB:-/opt/homebrew}" ;;
    *)        PM_PREFIX="" ;;
esac
QORE_API_VER=$(build/qore --module-api 2>/dev/null || true)
for p in "${PM_PREFIX}" /usr/local; do
    [ -z "$p" ] && continue
    [ -d "$p/lib/qore-modules/${QORE_API_VER}" ] && EXTRA_MODULE_DIRS="$p/lib/qore-modules/${QORE_API_VER}:${EXTRA_MODULE_DIRS}"
    [ -d "$p/lib/qore-modules" ] && EXTRA_MODULE_DIRS="$p/lib/qore-modules:${EXTRA_MODULE_DIRS}"
    [ -d "$p/share/qore-modules/${QORE_API_VER}" ] && EXTRA_MODULE_DIRS="$p/share/qore-modules/${QORE_API_VER}:${EXTRA_MODULE_DIRS}"
    [ -d "$p/share/qore-modules" ] && EXTRA_MODULE_DIRS="$p/share/qore-modules:${EXTRA_MODULE_DIRS}"
done
export QORE_MODULE_DIR="${QORE_SRC_DIR}/qlib:${EXTRA_MODULE_DIRS}${QORE_MODULE_DIR:-}"

# Run tests using the built qore binary
export PATH="${BUILD_DIR}:${PATH}"

# Skip max thread test on macOS - it can cause kernel panics on Apple Silicon
# when creating 8000+ threads overwhelms the pthread subsystem
export SKIP_MAX_THREAD_TEST=1

# Raise the file descriptor limit — macOS defaults to 256 which is too low for
# tests that create many sockets (e.g. AsyncSocketIo scalability test needs ~430)
ulimit -n 4096

# Run the test suite (QORE_TEST_OPTS is read from the environment by run_tests.sh)
./run_tests.sh

echo "=== macOS CI Tests Complete ==="
