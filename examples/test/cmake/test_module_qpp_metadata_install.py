#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Check that the binary modules built with Qore install their qpp API metadata, with CMAKE_EXECUTABLE (or cmake).

The language server reads the .meta.json files that qpp generates from a binary module's .qpp sources from the
installed metadata directory (QORE_METADATA_DIR, see design/api-metadata-for-modules.md).  The binary modules built
with Qore installed none: QORE_METADATA_DIR was set after the modules' add_subdirectory() calls, so
qore_install_qpp_metadata() installed nothing even for the modules that called it, and most modules, among them
linenoise, json and yaml, which installed their metadata as separate modules, did not call it.

The build test installs each module directory of the build (QORE_TEST_BUILD_DIR, by default build/) under a
temporary DESTDIR with the bootstrap install component, as install-qore-bootstrap and a full install do, and
requires every .meta.json file generated in that directory to be installed in the module's metadata directory.
"""

import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
BUILD = Path(os.environ.get("QORE_TEST_BUILD_DIR", ROOT / "build")).resolve()
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")
# the install component of the binary modules built with Qore (QORE_INSTALL_COMPONENT_BOOTSTRAP in CMakeLists.txt)
BOOTSTRAP_COMPONENT = "qore-bootstrap"


def qpp_modules():
    """returns the names of the module directories under modules/ whose binary module has qpp sources"""
    return sorted(path.parent.name for path in (ROOT / "modules").glob("*/CMakeLists.txt")
                  if "qore_wrap_qpp_value(" in strip_comments(path.read_text()))


def strip_comments(text):
    """returns CMake code without its line comments"""
    return re.sub(r"#[^\n]*", "", text)


class ModuleQppMetadataInstallTest(unittest.TestCase):
    def test_modules_with_qpp_sources_install_metadata(self):
        modules = qpp_modules()
        # the modules that installed their metadata as separate modules are among them
        for name in ("linenoise", "krb5", "json", "yaml"):
            self.assertIn(name, modules)
        for name in modules:
            code = strip_comments((ROOT / "modules" / name / "CMakeLists.txt").read_text())
            with self.subTest(module=name):
                wrap = re.search(r"qore_wrap_qpp_value\(([^)]*)\)", code)
                self.assertIsNotNone(wrap)
                metalist = re.search(r"\bMETALIST\s+(\w+)", wrap.group(1))
                self.assertIsNotNone(metalist, "qore_wrap_qpp_value() has no METALIST")
                self.assertRegex(code, r"qore_install_qpp_metadata\([^)]*\$\{" + metalist.group(1) + r"\}",
                                 "the METALIST files are not installed with qore_install_qpp_metadata()")

    def test_metadata_dir_is_set_before_the_modules(self):
        code = strip_comments((ROOT / "CMakeLists.txt").read_text())
        metadata_dir = re.search(r"set\(QORE_METADATA_DIR\b", code)
        first_module = re.search(r"add_subdirectory\(modules/", code)
        self.assertIsNotNone(metadata_dir)
        self.assertIsNotNone(first_module)
        self.assertLess(metadata_dir.start(), first_module.start(),
                        "QORE_METADATA_DIR is set after the first binary module, which then installs no metadata")

    def test_build_installs_module_metadata(self):
        if not (BUILD / "CMakeCache.txt").is_file():
            self.skipTest("no build with a CMake cache; set QORE_TEST_BUILD_DIR")
        checked = []
        for name in qpp_modules():
            module_build = BUILD / "modules" / name
            generated = sorted(path.name for path in module_build.glob("*.meta.json"))
            if not (module_build / "cmake_install.cmake").is_file() or not generated:
                # an optional module that this build did not make (such as mongodb without libmongoc)
                continue
            checked.append(name)
            with self.subTest(module=name):
                with tempfile.TemporaryDirectory(prefix="qore-qpp-metadata-") as destdir:
                    env = dict(os.environ, DESTDIR=destdir)
                    result = subprocess.run(
                        [CMAKE, f"-DCMAKE_INSTALL_COMPONENT={BOOTSTRAP_COMPONENT}", "-P",
                         str(module_build / "cmake_install.cmake")],
                        capture_output=True, text=True, timeout=300, env=env)
                    self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                    installed = {}
                    for path in Path(destdir).glob("**/qore/metadata/*/*.meta.json"):
                        installed.setdefault(path.parent.name, []).append(path.name)
                    self.assertEqual(1, len(installed), f"metadata directories installed: {sorted(installed)}")
                    directory, files = installed.popitem()
                    self.assertEqual(name, directory)
                    self.assertEqual(generated, sorted(files))
        # the modules that are always built with Qore were checked
        for name in ("linenoise", "krb5", "json", "yaml"):
            self.assertIn(name, checked)


if __name__ == "__main__":
    unittest.main()
