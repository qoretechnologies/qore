#!/bin/sh

set -e
set -x

# Build script for performance tests - compiles Qore with Release optimization.
# Used by the CI build stage; test-perf downloads the build/ artifact.

# Source environment from the Docker image (provides INSTALL_PREFIX, etc.)
ENV_FILE=/tmp/env.sh
if [ -f "${ENV_FILE}" ]; then
    . ${ENV_FILE}
fi

# setup QORE_SRC_DIR env var
cwd=`pwd`
if [ "${QORE_SRC_DIR}" = "" ]; then
    if [ -e "$cwd/qlib/SqlUtil.qm" ] || [ -e "$cwd/bin/qdbg" ] || [ -e "$cwd/cmake/QoreMacros.cmake" ] || [ -e "$cwd/lib/QoreLib.cpp" ]; then
        QORE_SRC_DIR=$cwd
    else
        QORE_SRC_DIR=$WORKDIR/qore
    fi
fi

export MAKE_JOBS=${MAKE_JOBS:-6}

if ! command -v pkg-config > /dev/null 2>&1 || ! pkg-config --exists krb5 krb5-gssapi; then
    echo && echo "-- installing Kerberos 5 development headers --"
    if command -v apt-get > /dev/null 2>&1; then
        apt-get update -qq && apt-get install -y -qq libkrb5-dev
    elif command -v apk > /dev/null 2>&1; then
        apk add --no-cache krb5-dev
    elif command -v dnf > /dev/null 2>&1; then
        dnf install -y krb5-devel
    elif command -v yum > /dev/null 2>&1; then
        yum install -y krb5-devel
    fi
fi

if [ ! -f /usr/share/eigen3/cmake/Eigen3Config.cmake ] \
    && [ ! -f /usr/lib/cmake/eigen3/Eigen3Config.cmake ] \
    && [ ! -f /usr/lib64/cmake/eigen3/Eigen3Config.cmake ] \
    && [ ! -f /usr/lib/x86_64-linux-gnu/cmake/eigen3/Eigen3Config.cmake ]; then
    echo && echo "-- installing Eigen3 development headers --"
    if command -v apt-get > /dev/null 2>&1; then
        apt-get update -qq && apt-get install -y -qq libeigen3-dev
    elif command -v apk > /dev/null 2>&1; then
        apk add --no-cache eigen-dev
    elif command -v dnf > /dev/null 2>&1; then
        dnf install -y eigen3-devel
    elif command -v yum > /dev/null 2>&1; then
        yum install -y eigen3-devel
    fi
fi

if ! command -v protoc > /dev/null 2>&1 \
    && [ ! -f /usr/lib/cmake/protobuf/protobuf-config.cmake ] \
    && [ ! -f /usr/lib64/cmake/protobuf/protobuf-config.cmake ] \
    && [ ! -f /usr/lib/x86_64-linux-gnu/cmake/protobuf/protobuf-config.cmake ]; then
    echo && echo "-- installing Protobuf development files --"
    if command -v apt-get > /dev/null 2>&1; then
        apt-get update -qq && apt-get install -y -qq libprotobuf-dev protobuf-compiler
    elif command -v apk > /dev/null 2>&1; then
        apk add --no-cache protobuf-dev
    elif command -v dnf > /dev/null 2>&1; then
        dnf install -y protobuf-devel protobuf-compiler
    elif command -v yum > /dev/null 2>&1; then
        yum install -y protobuf-devel protobuf-compiler
    fi
fi

if ! pkg-config --exists libutf8proc 2>/dev/null \
    && [ ! -f /usr/include/utf8proc.h ] \
    && [ ! -f /usr/local/include/utf8proc.h ]; then
    echo && echo "-- installing utf8proc development files --"
    if command -v apt-get > /dev/null 2>&1; then
        apt-get update -qq && apt-get install -y -qq libutf8proc-dev
    elif command -v apk > /dev/null 2>&1; then
        apk add --no-cache utf8proc-dev
    elif command -v dnf > /dev/null 2>&1; then
        dnf install -y utf8proc-devel
    elif command -v yum > /dev/null 2>&1; then
        yum install -y utf8proc-devel
    fi
fi

# install tree-sitter CLI for astparser module build
if ! command -v tree-sitter > /dev/null 2>&1; then
    echo && echo "-- installing tree-sitter CLI --"
    cargo install tree-sitter-cli@0.26.5
fi

# build Qore with Release optimization
echo && echo "-- building Qore (Release) --"
cd ${QORE_SRC_DIR}
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DSINGLE_COMPILATION_UNIT=1 -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}
make -j${MAKE_JOBS}

# Remove intermediate object files to reduce artifact size
find . -name "*.o" -delete 2>/dev/null || true
