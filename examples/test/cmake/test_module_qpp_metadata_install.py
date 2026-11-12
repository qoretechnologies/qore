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
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
BUILD = Path(os.environ.get("QORE_TEST_BUILD_DIR", ROOT / "build")).resolve()
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")
# the install component of the binary modules built with Qore (QORE_INSTALL_COMPONENT_BOOTSTRAP in CMakeLists.txt)
BOOTSTRAP_COMPONENT = "qore-bootstrap"
# the binary modules that are always built with Qore
ALWAYS_BUILT = ("linenoise", "krb5", "json", "yaml")


def qpp_modules():
    """returns the names of the module directories under modules/ whose binary module has qpp sources"""
    return sorted(path.parent.name for path in (ROOT / "modules").glob("*/CMakeLists.txt")
                  if "qore_wrap_qpp_value(" in strip_comments(path.read_text()))


def module_built(module_build):
    """returns True if the module directory of a build holds the built binary module"""
    return any(module_build.glob("*.qmod"))


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
        if not any(module_built(BUILD / "modules" / name) for name in ALWAYS_BUILT):
            # a build tree that is configured but whose binary modules were not built, such as one used only to
            # build the library
            self.skipTest(f"the binary modules of {BUILD} were not built; build them or set QORE_TEST_BUILD_DIR to "
                          "a full build")
        checked = []
        for name in qpp_modules():
            module_build = BUILD / "modules" / name
            if not (module_build / "cmake_install.cmake").is_file() or not module_built(module_build):
                # an optional module that this build did not make (such as mongodb without libmongoc)
                continue
            generated = sorted(path.name for path in module_build.glob("*.meta.json"))
            checked.append(name)
            with self.subTest(module=name):
                # a built module generates the metadata of its qpp sources
                self.assertTrue(generated, f"{name} was built but generated no qpp metadata")
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
        for name in ALWAYS_BUILT:
            self.assertIn(name, checked)


class BuildStateTest(unittest.TestCase):
    """Runs the build test against stand-in build trees with QORE_TEST_BUILD_DIR"""

    def run_build_test(self, build):
        env = dict(os.environ, QORE_TEST_BUILD_DIR=str(build))
        return subprocess.run(
            [sys.executable, "-B", "-W", "error", "-m", "unittest", "-v",
             f"{Path(__file__).stem}.ModuleQppMetadataInstallTest.test_build_installs_module_metadata"],
            cwd=Path(__file__).parent, capture_output=True, text=True, timeout=120, env=env)

    def make_build(self, root, built):
        """creates a configured build tree; with built, linenoise's binary module is there but no metadata"""
        (root / "CMakeCache.txt").touch()
        module_build = root / "modules" / "linenoise"
        module_build.mkdir(parents=True)
        (module_build / "cmake_install.cmake").write_text("")
        if built:
            (module_build / "linenoise-api-2.0.qmod").touch()

    def test_unbuilt_modules_skip(self):
        with tempfile.TemporaryDirectory(prefix="qore-qpp-build-") as directory:
            self.make_build(Path(directory), built=False)
            result = self.run_build_test(directory)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertIn("binary modules of", result.stderr)
            self.assertIn("were not built", result.stderr)

    def test_built_module_without_metadata_fails(self):
        with tempfile.TemporaryDirectory(prefix="qore-qpp-build-") as directory:
            self.make_build(Path(directory), built=True)
            result = self.run_build_test(directory)
            self.assertNotEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertIn("linenoise was built but generated no qpp metadata", result.stderr)


if __name__ == "__main__":
    unittest.main()
