#!/usr/bin/env python3
"""Verify that the documentation targets that generate Java bindings build with Ninja as with make (#7007).

The docs target of every module runs qjar to write the module's Java classes into the one ${CMAKE_BINARY_DIR}/java
tree.  The targets declared that directory as their BYPRODUCTS, and Ninja rejects a build in which more than one rule
produces the same output ("ninja: error: build.ninja:<n>: multiple rules generate java").  The targets name the
directory as a file to remove in a clean instead, and a clean with either generator still removes it.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")
MODULES = ("Alpha", "Beta", "Gamma")


class JavaBindingsOutputsTest(unittest.TestCase):
    def test_no_target_declares_the_shared_java_directory_as_an_output(self):
        text = (ROOT / "cmake" / "QoreMacros.cmake").read_text()
        self.assertIsNone(re.search(r"(BYPRODUCTS|OUTPUT)\s+\$\{CMAKE_BINARY_DIR\}/java\b", text),
                          "a target declares ${CMAKE_BINARY_DIR}/java, which the docs targets of all modules write, "
                          "as its output")

    def build(self, generator):
        with tempfile.TemporaryDirectory(prefix="qore-java-bindings-") as directory:
            root = Path(directory)
            source, build = root / "source", root / "build"
            (source / "qlib").mkdir(parents=True)
            for name in MODULES:
                (source / "qlib" / f"{name}.qm").write_text("%modern\n")
            # qdx writes the files the docs target reads; doxygen does nothing
            (source / "qdx.py").write_text('''import sys
from pathlib import Path
if "--post" in sys.argv:
    sys.exit(0)
if any(arg.endswith("Doxyfile.cmake.tmpl") for arg in sys.argv):
    index = next(i for i, arg in enumerate(sys.argv) if arg.endswith("Doxyfile.cmake.tmpl"))
    Path(sys.argv[index + 1]).write_text("QUIET = YES\\n")
else:
    output = Path(sys.argv[-1])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(Path(sys.argv[-2]).read_text())
''')
            (source / "doxygen.py").write_text("import sys\nsys.exit(0)\n")
            # qjar -i <dir> -m <module> writes the module's classes below <dir>, as the real one does
            (source / "qjar.py").write_text('''import sys
from pathlib import Path
root = Path(sys.argv[sys.argv.index("-i") + 1])
module = sys.argv[sys.argv.index("-m") + 1]
output = root / "qoremod" / module / (module + ".class")
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(module + "\\n")
''')
            python = Path(sys.executable).as_posix()
            modules = "\n".join(f'qore_user_module("qlib/{name}.qm")' for name in MODULES)
            (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.15...3.31)
project(JavaBindingsOutputs NONE)
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
set(QORE_BUILD_AOT_MODULES OFF)
set(QORE_GENERATE_JAVA_BINDINGS ON)
set(QORE_USER_MODULES_DIR share/qore-modules)
set(DOXYGEN_FOUND TRUE)
set(DOXYGEN_EXECUTABLE "{python}" "${{CMAKE_SOURCE_DIR}}/doxygen.py")
set(QORE_QDX_COMMAND "{python}" "${{CMAKE_SOURCE_DIR}}/qdx.py")
set(QORE_QJAR_COMMAND "{python}" "${{CMAKE_SOURCE_DIR}}/qjar.py")
set(QORE_DOC_TOOL_MODULES "")
foreach(target qore docs docs-lang docs-lib)
    add_custom_target(${{target}})
endforeach()
{modules}
''')
            result = subprocess.run([CMAKE, "-G", generator, "-S", str(source), "-B", str(build)],
                                    capture_output=True, text=True, timeout=120)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            # the docs targets of all modules, in parallel, then the fast docs target of one of them, which writes
            # the same module's classes again
            for target in ("docs", f"docs-fast-{MODULES[0]}"):
                result = subprocess.run([CMAKE, "--build", str(build), "--target", target, "--parallel", "4"],
                                        capture_output=True, text=True, timeout=120)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            java = build / "java"
            self.assertEqual(sorted(f"qoremod/{name}/{name}.class" for name in MODULES),
                             sorted(path.relative_to(java).as_posix() for path in java.rglob("*") if path.is_file()))
            # a clean removes the whole tree, as it did when the docs targets declared it as their BYPRODUCTS
            result = subprocess.run([CMAKE, "--build", str(build), "--target", "clean"],
                                    capture_output=True, text=True, timeout=120)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertFalse(java.exists(), "a clean leaves the Java bindings")

    def test_ninja(self):
        if not shutil.which("ninja"):
            self.skipTest("ninja is required for the Ninja generator")
        self.build("Ninja")

    def test_makefiles(self):
        if not shutil.which("make"):
            self.skipTest("make is required for the Unix Makefiles generator")
        self.build("Unix Makefiles")


if __name__ == "__main__":
    unittest.main()
