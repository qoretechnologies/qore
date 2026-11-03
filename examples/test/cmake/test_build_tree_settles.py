#!/usr/bin/env python3
"""Verify that a Qore build tree settles: with CMAKE_EXECUTABLE (or cmake), a build after a configure does not
configure again because of what the build itself wrote, and a configure that changes nothing rewrites no generated
source.

- QORE_USER_MODULE_RESOURCE_DIRS() watched the module directory itself with a CONFIGURE_DEPENDS glob, and the build
  links the module's AOT qmod into it (QORE_AOT_LINK_SOURCE_MODULES), so the build after a configure ran configure
  again, which rewrote the generated sources below;
- the single-compilation-unit sources of the in-tree binary modules were written with file(WRITE) on every configure,
  so each module was compiled and linked again, and the qlib AOT modules that require it after it.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")


class BuildTreeSettlesTest(unittest.TestCase):
    def setUp(self):
        if not shutil.which(CMAKE):
            self.skipTest("CMake is required")
        if not shutil.which("make") and not shutil.which("gmake"):
            self.skipTest("make is required for the Makefile generator")
        directory = tempfile.TemporaryDirectory(prefix="qore-build-settles-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.source, self.build = self.root / "source", self.root / "build"
        self.source.mkdir()

    def run_cmake(self, *args):
        result = subprocess.run([CMAKE, *map(str, args)], capture_output=True, text=True, timeout=120)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        return result.stdout + result.stderr

    def configure(self):
        return self.run_cmake("-G", "Unix Makefiles", "-S", self.source, "-B", self.build)

    def build_all(self):
        return self.run_cmake("--build", self.build)

    def test_resource_dirs_ignore_the_linked_qmod(self):
        module = self.source / "qlib" / "Probe"
        (module / "data").mkdir(parents=True)
        (module / "Probe.qm").write_text("%modern\n")
        (module / "data" / "schema.json").write_text("{}\n")
        (self.source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(ResourceDirsWatch NONE)
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
QORE_USER_MODULE_RESOURCE_DIRS(_dirs "${{CMAKE_SOURCE_DIR}}/qlib/Probe")
message(STATUS "resource dirs: ${{_dirs}}")
add_custom_target(probe ALL)
''')
        self.assertIn("resource dirs: data", self.configure())
        # what the build links into the module directory after the configure
        (module / "Probe.qmod").write_text("qmod\n")
        out = self.build_all()
        self.assertNotIn("GLOB mismatch", out)
        self.assertNotIn("resource dirs:", out)

        # negative: a new resource directory with a file configures again, and is found
        (module / "logos").mkdir()
        (module / "logos" / "logo.svg").write_text("<svg/>\n")
        out = self.build_all()
        self.assertIn("resource dirs: data;logos", out)
        # and a directory with sources is no resource directory
        (module / "impl").mkdir()
        (module / "impl" / "Impl.qc").write_text("%modern\n")
        out = self.build_all()
        self.assertIn("resource dirs: data;logos", out)
        self.assertNotIn("impl", out.split("resource dirs:")[1].split("\n")[0])
        self.assertNotIn("GLOB mismatch", self.build_all())

    def test_generated_source_is_rewritten_only_when_it_changes(self):
        (self.source / "content.txt").write_text("int a;\n")
        (self.source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(GeneratedSource NONE)
include("{ROOT.as_posix()}/cmake/QoreMacrosIntern.cmake")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${{CMAKE_SOURCE_DIR}}/content.txt")
file(READ "${{CMAKE_SOURCE_DIR}}/content.txt" _content)
qore_write_generated_source("${{CMAKE_BINARY_DIR}}/scu.cpp" "${{_content}}")
''')
        self.configure()
        generated = self.build / "scu.cpp"
        self.assertEqual("int a;\n", generated.read_text())
        # a time before the configure below, so that a rewrite is seen even within the file system's resolution
        os.utime(generated, ns=(1_000_000_000, 1_000_000_000))
        self.configure()
        self.assertEqual(1_000_000_000, generated.stat().st_mtime_ns)

        # negative: a changed content is written
        (self.source / "content.txt").write_text("int b;\n")
        self.configure()
        self.assertEqual("int b;\n", generated.read_text())
        self.assertNotEqual(1_000_000_000, generated.stat().st_mtime_ns)


if __name__ == "__main__":
    unittest.main()
