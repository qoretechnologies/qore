#!/bin/sh
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
# Source from the module source root in each RPM phase.
# shellcheck source=build-env.sh
. /usr/lib/rpm/qore/build-env.sh
qore_stdlib_paths=$(env -u QORE_MODULE_DIR_ONLY /usr/bin/qore --module-path)
export QORE_MODULE_DIR="$PWD/build:$PWD/build/qlib-qmod:$qore_stdlib_paths"
