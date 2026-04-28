#!/usr/bin/env bash
# Copyright (C) 2026 Qore Technologies, s.r.o.

set -euxo pipefail

module_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
repo_dir="$(cd "$module_dir/../.." && pwd)"
build_dir="${BUILD_DIR:-$repo_dir/build}"
env_file=/tmp/env.sh

if [[ -f "${env_file}" ]]; then
    # shellcheck disable=SC1090
    set +u
    . "${env_file}"
    set -u
fi

installed_qore="$(command -v qore || true)"
qore_prefix="${INSTALL_PREFIX:-/usr}"
if [[ -n "${installed_qore}" ]]; then
    qore_prefix="${INSTALL_PREFIX:-$(dirname "$(dirname "$installed_qore")")}"
fi

uname -a
if [[ -n "${installed_qore}" ]]; then
    "$installed_qore" --version
fi
cmake --version
pkg-config --modversion krb5
pkg-config --modversion krb5-gssapi

cmake -S "$repo_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX="$qore_prefix"
cmake --build "$build_dir"

qore_bin="${QORE_BIN:-$build_dir/qore}"
export LD_LIBRARY_PATH="$build_dir:${LD_LIBRARY_PATH:-}"
export QORE_MODULE_DIR="$repo_dir/qlib:$build_dir/modules/krb5:$build_dir/modules/logger_bin:${QORE_MODULE_DIR:-}"
cd "$module_dir/test"
"$qore_bin" --enable-debug krb5.qtest -v

if command -v krb5kdc >/dev/null && command -v kadmin.local >/dev/null && command -v kdb5_util >/dev/null; then
    "$module_dir/test/run-gss-kdc-integration.sh"
else
    echo "skipping GSS/KDC integration test: krb5kdc, kadmin.local, or kdb5_util not available"
fi
