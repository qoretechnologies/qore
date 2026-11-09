#!/usr/bin/env python3
"""Tests that a binary module build removes the files of the module built for another module API.

A binary module is built as <name>-api-<major>.<minor>.qmod (QORE_API_VERSION, the module API of the Qore headers);
when the API changes, CMake writes the new file but does not remove the old one, which then stays in an incrementally
built tree.  qore_remove_stale_module_api_files() (cmake/QoreMacros.cmake, called by the QORE_BINARY_MODULE* macros)
removes, after each build of a module, the other <name>-api-<major>.<minor>.qmod files of the module in its output
directory and nothing else (cmake/QoreRemoveStaleModuleApiFiles.cmake).

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")
SCRIPT = ROOT / "cmake/QoreRemoveStaleModuleApiFiles.cmake"


class StaleModuleApiFilesTest(unittest.TestCase):
    def run_command(self, command, cwd):
        result = subprocess.run(command, cwd=cwd, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, f"{command}\n{result.stdout}\n{result.stderr}")
        return result

    def write_project(self, source):
        (source / "cmake").mkdir(parents=True)
        (source / "cmake/cmake_uninstall.cmake.in").write_text("# fixture\n")
        (source / "fixture.c").write_text("void fixture(void) {}\n")
        (source / "Doxyfile.in").write_text("PROJECT_NAME = Fixture\n")
        # "fixture" is built by the module macro and has the name of its target; "other" sets its suffix itself and
        # calls the function, as a module that does not use the macro does, and has an OUTPUT_NAME of its own
        (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(StaleModuleApiFiles C)
include("{ROOT}/cmake/QoreMacros.cmake")
set(QORE_USERMODULE_DOXYGEN_TEMPLATE "${{CMAKE_SOURCE_DIR}}/Doxyfile.in")
set(QORE_MODULES_DIR lib/qore-modules)
set(QORE_GENERATE_JAVA_BINDINGS OFF)
add_library(fixture MODULE fixture.c)
qore_binary_module_intern2(fixture 1.0 "" "2")
add_library(other MODULE fixture.c)
set_target_properties(other PROPERTIES PREFIX "" SUFFIX "-api-${{QORE_API_VERSION}}.qmod" OUTPUT_NAME renamed
    LIBRARY_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/renamed")
qore_remove_stale_module_api_files(other)
''')

    def build(self, root, source, build, api):
        self.run_command([CMAKE, "-S", str(source), "-B", str(build), f"-DQORE_API_VERSION={api}"], root)
        return self.run_command([CMAKE, "--build", str(build), "--target", "fixture", "other"], root)

    def test_api_change_removes_only_the_stale_module_files(self):
        with tempfile.TemporaryDirectory(prefix="qore stale api ") as directory:
            root = Path(directory)
            source = root / "source"
            build = root / "build"
            self.write_project(source)
            self.build(root, source, build, "2.0")
            self.assertTrue((build / "fixture-api-2.0.qmod").is_file())
            self.assertTrue((build / "renamed/renamed-api-2.0.qmod").is_file())

            # files that are not a build of the module for another module API, and another module API's file that
            # is only a link
            kept = ["fixture.qmod", "fixture-api-2.1x.qmod", "fixture-api-.qmod", "fixture-api-2.qmod",
                    "fixture-api-2.0.qm", "fixturex-api-1.0.qmod", "other-api-1.0.qmod", "renamed-api-1.0.qmod",
                    "renamed/fixture-api-1.0.qmod", "renamed/other-api-1.0.qmod"]
            for name in kept:
                (build / name).write_text("kept\n")
            (build / "fixture-api-1.5.qmod").symlink_to(build / "fixture-api-2.0.qmod")

            out = self.build(root, source, build, "2.1")
            self.assertTrue((build / "fixture-api-2.1.qmod").is_file())
            self.assertTrue((build / "renamed/renamed-api-2.1.qmod").is_file())
            for name in ("fixture-api-2.0.qmod", "fixture-api-1.5.qmod", "renamed/renamed-api-2.0.qmod"):
                with self.subTest(removed=name):
                    self.assertFalse(os.path.lexists(build / name))
                    self.assertIn(str(build / name), out.stdout)
            for name in kept:
                with self.subTest(kept=name):
                    self.assertEqual((build / name).read_text(), "kept\n")

            # a build that changes nothing removes nothing
            (build / "fixture-api-1.0.qmod").write_text("stale\n")
            self.run_command([CMAKE, "--build", str(build), "--target", "fixture"], root)
            self.assertTrue((build / "fixture-api-1.0.qmod").is_file(), "the step runs only when the module is built")
            (source / "fixture.c").write_text("void fixture(void) { }\n")
            self.run_command([CMAKE, "--build", str(build), "--target", "fixture"], root)
            self.assertFalse((build / "fixture-api-1.0.qmod").exists())
            self.assertTrue((build / "fixture-api-2.1.qmod").is_file())

    def test_script_requires_its_arguments(self):
        with tempfile.TemporaryDirectory(prefix="qore stale api ") as directory:
            root = Path(directory)
            (root / "mod-api-2.0.qmod").write_text("stale\n")
            for args in ([], [f"-DDIR={root}", "-DNAME=mod"], [f"-DDIR={root}", "-DKEEP=mod-api-2.1.qmod"],
                         ["-DNAME=mod", "-DKEEP=mod-api-2.1.qmod"]):
                with self.subTest(args=args):
                    result = subprocess.run([CMAKE] + args + ["-P", str(SCRIPT)], capture_output=True, text=True)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("is required", result.stderr)
                    self.assertTrue((root / "mod-api-2.0.qmod").is_file())
            self.run_command([CMAKE, f"-DDIR={root}", "-DNAME=mod", "-DKEEP=mod-api-2.1.qmod", "-P", str(SCRIPT)],
                             root)
            self.assertFalse((root / "mod-api-2.0.qmod").exists())


if __name__ == "__main__":
    unittest.main()
