#!/usr/bin/env python3
"""Verify that a qlib AOT module's build dependencies are what its current depfile names, with CMAKE_EXECUTABLE (or
cmake) and the Makefile generator.

The Makefile generators before CMake 4.0 append a rewritten depfile to the dependencies they consolidated from it
before, so a file that a module depended on once stayed a dependency of it: once that file was removed (a binary
module that moved from the installed prefix into the Qore build), the module was compiled again in every build.  A
stand-in for qcc writes the module and a depfile that names the dependencies listed in a file of the test.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")

# writes the -o output and the --depfile, which names the module source and the dependencies listed in deps.txt
FAKE_QCC = r'''import sys
from pathlib import Path
args = sys.argv[1:]
output = Path(args[args.index("-o") + 1])
depfile = Path(next(arg for arg in args if arg.startswith("--depfile=")).split("=", 1)[1])
source = Path(args[args.index("-m") + 1])
deps = [line for line in (Path(__file__).parent / "deps.txt").read_text().splitlines() if line]
with open(Path(__file__).parent / "compiles.log", "a") as log:
    log.write(output.name + "\n")
output.write_text("qmod\n")
depfile.write_text(f"{output}: \\\n    " + " \\\n    ".join([str(source)] + deps) + "\n")
'''


class QmodDepfileDependenciesTest(unittest.TestCase):
    def setUp(self):
        if not shutil.which(CMAKE):
            self.skipTest("CMake is required")
        if not shutil.which("make") and not shutil.which("gmake"):
            self.skipTest("make is required for the Makefile generator")
        directory = tempfile.TemporaryDirectory(prefix="qore-qmod-depfile-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.source, self.build = self.root / "source", self.root / "build"
        (self.source / "qlib").mkdir(parents=True)
        (self.source / "qlib" / "Probe.qm").write_text("%modern\n")
        (self.source / "qcc.py").write_text(FAKE_QCC)
        qcc = self.source / "qcc"
        qcc.write_text(f'#!/bin/sh\nexec "{sys.executable}" "{self.source / "qcc.py"}" "$@"\n')
        qcc.chmod(0o755)
        (self.source / "qcc-format.stamp").write_text("format\n")
        (self.source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(QmodDepfileDependencies NONE)
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
set(QORE_BUILD_AOT_MODULES ON)
set(QORE_QCC_EXECUTABLE "{qcc.as_posix()}")
set(QCC_FORMAT_STAMP "${{CMAKE_SOURCE_DIR}}/qcc-format.stamp")
set(QORE_AOT_LINK_SOURCE_MODULES OFF)
set(QORE_GENERATE_JAVA_BINDINGS OFF)
set(QORE_USER_MODULES_DIR share/qore-modules)
foreach(target qore docs docs-lang docs-lib)
    add_custom_target(${{target}})
endforeach()
qore_user_module("qlib/Probe.qm")
QORE_FINALIZE_USER_MODULE_DEPENDENCIES()
''')
        self.deps = [self.root / name for name in ("installed-api.qmod", "built-api.qmod")]
        for dep in self.deps:
            dep.write_text("api\n")

    def set_deps(self, *deps):
        (self.source / "deps.txt").write_text("".join(f"{dep}\n" for dep in deps))

    def compiles(self):
        log = self.source / "compiles.log"
        return log.read_text().splitlines() if log.exists() else []

    def build_probe(self):
        """Builds the module and returns the number of times it was compiled by that build"""
        before = len(self.compiles())
        result = subprocess.run([CMAKE, "--build", str(self.build), "--target", "Probe-qmod"],
                                capture_output=True, text=True, timeout=120)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        return len(self.compiles()) - before

    def test_removed_dependency_is_dropped(self):
        installed, built = self.deps
        self.set_deps(installed)
        result = subprocess.run([CMAKE, "-G", "Unix Makefiles", "-S", str(self.source), "-B", str(self.build)],
                                capture_output=True, text=True, timeout=60)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertEqual(1, self.build_probe())
        # nothing changed: nothing is compiled
        self.assertEqual(0, self.build_probe())

        # the module now loads a different copy of the binary module, and is compiled again when the old one changes
        self.set_deps(built)
        os.utime(installed, None)
        self.assertEqual(1, self.build_probe())
        self.assertEqual(0, self.build_probe())

        # the copy it no longer loads is removed: it is no dependency, so nothing is compiled again
        installed.unlink()
        self.assertEqual(0, self.build_probe())
        self.assertEqual(0, self.build_probe())
        # the copy it loads is still one
        os.utime(built, None)
        self.assertEqual(1, self.build_probe())
        self.assertEqual(0, self.build_probe())

        # the consolidated dependencies hold one copy of the depfile's dependencies, not one per compile
        depend = self.build / "CMakeFiles" / "Probe-qmod.dir" / "compiler_depend.make"
        text = depend.read_text()
        self.assertEqual(1, text.count(f"{built}:"))
        self.assertNotIn(str(installed), text)

    def test_install_removes_depfiles_beside_installed_modules(self):
        """A depfile and its fingerprints describe the built module; none is left beside an installed copy"""
        (self.source / "qlib" / "Split").mkdir()
        (self.source / "qlib" / "Split" / "Split.qm").write_text("%modern\n")
        cmakelists = self.source / "CMakeLists.txt"
        cmakelists.write_text(cmakelists.read_text().replace(
            'qore_user_module("qlib/Probe.qm")',
            'set(QORE_AOT_MODULES_DIR lib/aot)\nqore_user_module("qlib/Probe.qm")\nqore_user_module("qlib/Split")'))
        self.set_deps()
        prefix = self.root / "prefix"
        result = subprocess.run([CMAKE, "-G", "Unix Makefiles", "-S", str(self.source), "-B", str(self.build),
                                 f"-DCMAKE_INSTALL_PREFIX={prefix}"],
                                capture_output=True, text=True, timeout=60)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        result = subprocess.run([CMAKE, "--build", str(self.build)], capture_output=True, text=True, timeout=120)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        # where the module's AOT rule publishes each qmod, as the first install shows
        result = subprocess.run([CMAKE, "--install", str(self.build)], capture_output=True, text=True, timeout=120)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        qmods = sorted(prefix.rglob("*.qmod"))
        self.assertEqual(["Probe.qmod", "Split.qmod"], sorted(qmod.name for qmod in qmods))
        orphans = []
        for qmod in qmods:
            for suffix in (".d", ".d.fingerprints"):
                orphan = qmod.with_name(qmod.name + suffix)
                orphan.write_text(f"{qmod}: {self.source}\n")
                orphans.append(orphan)
        result = subprocess.run([CMAKE, "--install", str(self.build)], capture_output=True, text=True, timeout=120)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertEqual([], [str(orphan) for orphan in orphans if orphan.exists()])
        # the modules themselves stay
        self.assertEqual([], [str(qmod) for qmod in qmods if not qmod.is_file()])


if __name__ == "__main__":
    unittest.main()
