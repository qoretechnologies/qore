#!/bin/sh
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
# Source this file in each build/check section; no ambient Qore installation
# or developer dependency prefix may determine package contents.
unset CMAKE_PREFIX_PATH LD_LIBRARY_PATH LD_PRELOAD PKG_CONFIG_PATH
unset QORE_MODULE_DIR QORE_INCLUDE_DIR QORE_INCLUDE_DIRS QORE_LIBDIR
unset QORE_AOT_LINK_CONF QORE_AOT_BIG_FN_THRESHOLD QORE_AOT_NO_DEBUG_INFO
unset QORE_DOC_DEFINES QCC_SPLIT_THRESHOLD DOXYGEN_EXECUTABLE
export QORE_MODULE_DIR_ONLY=1
export QCC_JOBS=1
export TZ=UTC
export LC_ALL=C.UTF-8

# Call in %build after the distro flags have been initialized. Mapping to
# RPM's final debug-source location lets debugedit collect the source files;
# mapping to '.' instead produces empty debugsource RPMs.
qore_set_source_prefix_maps() {
    qore_prefix_flag=$(python3 -c '
import os, shlex, sys
source, destination = os.getcwd(), sys.argv[1]
if "=" in source or any(c in source + destination for c in "\r\n"):
    raise SystemExit("RPM source-prefix maps cannot contain line breaks or an equals sign in the source path")
if (not destination.startswith("/usr/src/debug/")
        or any(part in ("", ".", "..") for part in destination.split("/")[4:])):
    raise SystemExit("RPM source-prefix maps require a directory below /usr/src/debug")
print(shlex.quote("-ffile-prefix-map=" + source + "=" + destination))
' "$1") || return
    CFLAGS="${CFLAGS:-} $qore_prefix_flag"
    CXXFLAGS="${CXXFLAGS:-} $qore_prefix_flag"
    export CFLAGS CXXFLAGS
}
