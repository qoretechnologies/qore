#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Compiler flags for test programs that are built with the headers of a Qore build and linked with its library.

A program linked with a build's libqore must target the same platform version: on macOS the linker warns when it
links an object built for an older deployment target than the library's (the compiler's default is the running
system's version, while the build uses CMAKE_OSX_DEPLOYMENT_TARGET).  A program that includes libqore's internal
headers also needs the include directories of the third-party headers they include, which are not in the compiler's
default search path on every system (MacPorts and Homebrew install them under their own prefixes).  Both come from the
build's CMake cache.
"""

from pathlib import Path
import sys

# the cache entries of the third-party include directories of libqore's headers (see the libqore target's include
# directories in CMakeLists.txt)
INCLUDE_DIR_ENTRIES = ("OPENSSL_INCLUDE_DIR", "MPFR_INCLUDE_DIR", "MPFRGMP_INCLUDE_DIR", "PCRE2_INCLUDE_DIRS",
                       "ZLIB_INCLUDE_DIR", "BZIP2_INCLUDE_DIR", "ICONV_INCLUDE_DIR")
# directories the compiler searches anyway: naming one with -I would reorder the system headers (#include_next)
DEFAULT_INCLUDE_DIRS = ("/usr/include", "/usr/local/include")


def cmake_cache(build):
    """Returns the entries of the CMake cache of the given build directory, which must have one"""
    entries = {}
    for line in (Path(build) / "CMakeCache.txt").read_text().splitlines():
        if not line or line.startswith(("#", "//")) or "=" not in line:
            continue
        key, value = line.split("=", 1)
        entries[key.split(":", 1)[0]] = value
    return entries


def target_flags(build, platform=sys.platform):
    """Returns the flags that make a program target the platform version of the given build"""
    target = cmake_cache(build).get("CMAKE_OSX_DEPLOYMENT_TARGET", "")
    if platform == "darwin" and target:
        return ["-mmacosx-version-min=" + target]
    return []


def include_flags(build):
    """Returns the include flags of the third-party headers that libqore's headers include, in the given build"""
    cache = cmake_cache(build)
    flags = []
    for entry in INCLUDE_DIR_ENTRIES:
        for directory in cache.get(entry, "").split(";"):
            if (directory and not directory.endswith("-NOTFOUND") and directory not in DEFAULT_INCLUDE_DIRS
                    and "-I" + directory not in flags):
                flags.append("-I" + directory)
    return flags
