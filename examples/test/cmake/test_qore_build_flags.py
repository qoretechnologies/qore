#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Check the flags that test programs built with a Qore build's headers and library take from its CMake cache."""

import os
from pathlib import Path
import tempfile
import unittest

import qore_build_flags

ROOT = Path(__file__).resolve().parents[3]
BUILD = Path(os.environ.get("QORE_TEST_BUILD_DIR", ROOT / "build")).resolve()


class QoreBuildFlagsTest(unittest.TestCase):
    def cache(self, text):
        temporary = tempfile.TemporaryDirectory(prefix="qore-build-flags-")
        self.addCleanup(temporary.cleanup)
        (Path(temporary.name) / "CMakeCache.txt").write_text(text)
        return temporary.name

    def test_cache_entries_types_and_comments(self):
        build = self.cache('''# This is the CMakeCache file.
//Path to a file.
OPENSSL_INCLUDE_DIR:PATH=/opt/local/include
CMAKE_OSX_DEPLOYMENT_TARGET:STRING=26.6
PCRE2_INCLUDE_DIRS:INTERNAL=/opt/pcre2/include;/opt/pcre2/include/extra
EMPTY:STRING=
VALUE_WITH_EQUALS:STRING=a=b

UNTYPED=value
''')
        cache = qore_build_flags.cmake_cache(build)
        self.assertEqual("/opt/local/include", cache["OPENSSL_INCLUDE_DIR"])
        self.assertEqual("26.6", cache["CMAKE_OSX_DEPLOYMENT_TARGET"])
        self.assertEqual("", cache["EMPTY"])
        self.assertEqual("a=b", cache["VALUE_WITH_EQUALS"])
        self.assertEqual("value", cache["UNTYPED"])
        # comments, help lines and blank lines are not entries
        self.assertEqual({"OPENSSL_INCLUDE_DIR", "CMAKE_OSX_DEPLOYMENT_TARGET", "PCRE2_INCLUDE_DIRS", "EMPTY",
                          "VALUE_WITH_EQUALS", "UNTYPED"}, set(cache))

    def test_deployment_target_only_on_macos_and_when_set(self):
        build = self.cache("CMAKE_OSX_DEPLOYMENT_TARGET:STRING=26.6\n")
        self.assertEqual(["-mmacosx-version-min=26.6"], qore_build_flags.target_flags(build, "darwin"))
        self.assertEqual([], qore_build_flags.target_flags(build, "linux"))
        self.assertEqual([], qore_build_flags.target_flags(self.cache("CMAKE_OSX_DEPLOYMENT_TARGET:STRING=\n"),
                                                           "darwin"))
        self.assertEqual([], qore_build_flags.target_flags(self.cache("CMAKE_BUILD_TYPE:STRING=debug\n"), "darwin"))

    def test_include_dirs_skip_missing_default_and_repeated_dirs(self):
        build = self.cache('''OPENSSL_INCLUDE_DIR:PATH=/opt/local/include
MPFR_INCLUDE_DIR:PATH=/opt/local/include
MPFRGMP_INCLUDE_DIR:PATH=/usr/include
PCRE2_INCLUDE_DIRS:INTERNAL=/opt/pcre2/include;/usr/local/include
ZLIB_INCLUDE_DIR:PATH=ZLIB_INCLUDE_DIR-NOTFOUND
BZIP2_INCLUDE_DIR:PATH=
UNRELATED_INCLUDE_DIR:PATH=/opt/unrelated/include
''')
        self.assertEqual(["-I/opt/local/include", "-I/opt/pcre2/include"], qore_build_flags.include_flags(build))

    def test_build_cache(self):
        # the flags of the build the other tests use: its third-party include directories exist
        if not (BUILD / "CMakeCache.txt").is_file():
            self.skipTest("no build with a CMake cache; set QORE_TEST_BUILD_DIR")
        for flag in qore_build_flags.include_flags(BUILD):
            self.assertTrue(Path(flag[2:]).is_dir(), flag)
        self.assertIn("OPENSSL_INCLUDE_DIR", qore_build_flags.cmake_cache(BUILD))


if __name__ == "__main__":
    unittest.main()
