#!/bin/sh
# The development files that a build of Qore needs and that a CI image may lack: sourced by build.sh and
# build-separate-units.sh, which call qore_ci_install_build_deps

qore_ci_install_build_deps() {
    if ! command -v pkg-config > /dev/null 2>&1 || ! pkg-config --exists krb5 krb5-gssapi libcares; then
        echo && echo "-- installing Kerberos 5 and c-ares development headers --"
        if command -v apt-get > /dev/null 2>&1; then
            apt-get update -qq && apt-get install -y -qq libkrb5-dev libcares-dev
        elif command -v apk > /dev/null 2>&1; then
            apk add --no-cache krb5-dev c-ares-dev
        elif command -v dnf > /dev/null 2>&1; then
            dnf install -y krb5-devel c-ares-devel
        elif command -v yum > /dev/null 2>&1; then
            yum install -y krb5-devel c-ares-devel
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
}
