#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Check that an edit made while an object compiles is compiled by the next build (issue #6951).

A compiler reads its inputs at the start and writes the object at the end; make and Ninja compare the times after
the fact, so an input edited in between is older than the object, which holds the code from before the edit, and the
edit was never compiled (a single-compilation-unit object of libqore compiles for minutes).
qore_compile_race_guard() (cmake/QoreCompileRaceGuard.cmake, tools/compile-race-guard.py) makes such an object out of
date; see design/compile-race-guard.md.

The toy project compiles a unit that includes another source, like the single-compilation-unit wrapper of libqore.
Its compiler launcher (the place of ccache, inside the guard) compiles to a temporary file, then waits on a FIFO
latch while the test edits the included source, and only then writes the object: the race, made deterministic.
"""

import os
from pathlib import Path
import select
import shutil
import subprocess
import sys
import tempfile
import textwrap
import time
import unittest

ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")
GUARD_CMAKE = (ROOT / "cmake/QoreCompileRaceGuard.cmake").as_posix()
GUARD_TOOL = ROOT / "tools/compile-race-guard.py"
TIMEOUT = 120

# The compiler launcher of the toy project: armed by the file "arm" in its directory, compiles to a temporary file,
# reports on the FIFO "ready", waits for a line on the FIFO "go", and then writes the object (read early, write late).
SLOW_COMPILER = r'''
import os, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
args = sys.argv[1:]
arm = os.path.join(here, "arm")
if "-o" not in args or not os.path.exists(arm):
    os.execvp(args[0], args)
os.unlink(arm)
i = args.index("-o")
obj = args[i + 1]
tmp = obj + ".slow"
status = subprocess.call(args[:i + 1] + [tmp] + args[i + 2:])
with open(os.path.join(here, "ready"), "w") as f:
    f.write("compiled\n")
with open(os.path.join(here, "go")) as f:
    f.read()
if status == 0:
    with open(tmp, "rb") as f:
        data = f.read()
    os.unlink(tmp)
    with open(obj, "wb") as f:
        f.write(data)
sys.exit(status)
'''


def member(value):
    return f"int member_value() {{ return {value}; }}\n"


class CompileRaceGuardTest(unittest.TestCase):
    def setUp(self):
        if not shutil.which(CMAKE):
            self.skipTest("CMake is required")
        directory = tempfile.TemporaryDirectory(prefix="qore-compile-race-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)

    def run_cmake(self, *args):
        result = subprocess.run([CMAKE, *map(str, args)], capture_output=True, text=True, timeout=TIMEOUT)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        return result.stdout + result.stderr

    def make_project(self, name, generator, guard):
        project = self.root / name
        source, build, tools = project / "source", project / "build", project / "tools"
        source.mkdir(parents=True)
        tools.mkdir()
        (tools / "slow-compiler.py").write_text(SLOW_COMPILER)
        os.mkfifo(tools / "ready")
        os.mkfifo(tools / "go")
        (source / "member.cpp").write_text(member(1))
        (source / "unit.cpp").write_text(textwrap.dedent('''\
            #include <cstdio>
            #include "member.cpp"
            int main() { std::printf("%d\\n", member_value()); return 0; }
            '''))
        (source / "CMakeLists.txt").write_text(textwrap.dedent(f'''\
            cmake_minimum_required(VERSION 3.14...3.31)
            project(CompileRaceGuard CXX)
            include("{GUARD_CMAKE}")
            add_executable(probe unit.cpp)
            qore_compile_race_guard(probe)
            '''))
        self.run_cmake("-G", generator, "-S", source, "-B", build, "-DCMAKE_BUILD_TYPE=Debug",
                       f"-DQORE_COMPILE_RACE_GUARD={'ON' if guard else 'OFF'}",
                       f"-DCMAKE_CXX_COMPILER_LAUNCHER={sys.executable};{tools / 'slow-compiler.py'}")
        return source, build, tools

    def probe(self, build):
        exe = build / "probe"
        result = subprocess.run([exe], capture_output=True, text=True, timeout=TIMEOUT)
        self.assertEqual(0, result.returncode, result.stderr)
        return int(result.stdout)

    def build_with_edit_during_compile(self, source, build, tools, value):
        """Builds with the slow compile armed, and writes member(value) while the object is compiling."""
        (tools / "arm").write_text("")
        ready = os.open(tools / "ready", os.O_RDONLY | os.O_NONBLOCK)
        try:
            proc = subprocess.Popen([CMAKE, "--build", str(build)], stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True)
            try:
                deadline = time.monotonic() + TIMEOUT
                while True:
                    readable, _, _ = select.select([ready], [], [], 1)
                    if readable and os.read(ready, 100):
                        break
                    if proc.poll() is not None:
                        self.fail("the build ended before the compile: " + proc.stdout.read())
                    self.assertLess(time.monotonic(), deadline, "the compile did not start")
                (source / "member.cpp").write_text(member(value))
                with open(tools / "go", "w") as go:
                    go.write("go\n")
                out, _ = proc.communicate(timeout=TIMEOUT)
            finally:
                if proc.poll() is None:
                    proc.kill()
                    proc.communicate()
            self.assertEqual(0, proc.returncode, out)
        finally:
            os.close(ready)
        self.assertFalse((tools / "arm").exists(), "the object was not compiled")

    def check(self, generator, guard):
        source, build, tools = self.make_project(f"{generator.replace(' ', '-')}-{guard}", generator, guard)
        self.run_cmake("--build", build)
        self.assertEqual(1, self.probe(build))
        # an incremental build, during which member.cpp is edited again
        (source / "member.cpp").write_text(member(2))
        self.build_with_edit_during_compile(source, build, tools, 3)
        if not guard:
            # the object holds the code read at the start of its compile (with the guard, it is older than the
            # previous link when that was in the same second, and the probe is not linked again in this build)
            self.assertEqual(2, self.probe(build))
        self.run_cmake("--build", build)
        value = self.probe(build)
        # and the tree settles
        self.assertNotIn("Building CXX object", self.run_cmake("--build", build))
        return value

    def test_makefiles(self):
        self.assertEqual(3, self.check("Unix Makefiles", True), "the edit made during the compile was lost")

    def test_makefiles_without_guard_loses_the_edit(self):
        # the race itself, as the toy reproduces it: without the guard the edit is never compiled
        self.assertEqual(2, self.check("Unix Makefiles", False))

    def test_ninja(self):
        if not shutil.which("ninja"):
            self.skipTest("ninja is required for the Ninja generator")
        self.assertEqual(3, self.check("Ninja", True), "the edit made during the compile was lost")

    def test_no_change_no_rebuild(self):
        # an object whose inputs did not change during its compile keeps its time: a second build compiles nothing
        source, build, tools = self.make_project("settled", "Unix Makefiles", True)
        self.run_cmake("--build", build)
        obj = next(build.rglob("unit.cpp.o"))
        mtime = obj.stat().st_mtime_ns
        self.assertGreater(mtime, (source / "member.cpp").stat().st_mtime_ns)
        out = self.run_cmake("--build", build)
        self.assertNotIn("Building CXX object", out)
        self.assertEqual(mtime, obj.stat().st_mtime_ns)


class DepfileTest(unittest.TestCase):
    def setUp(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location("compile_race_guard", GUARD_TOOL)
        self.guard = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.guard)

    def test_depfile_inputs(self):
        with tempfile.TemporaryDirectory(prefix="qore-compile-race-") as directory:
            path = Path(directory) / "x.o.d"
            path.write_text("dir/x.o: x.cpp /inc/a\\ b.h \\\n  c$$d.h \\\n  e.h\nx.h:\n")
            self.assertEqual(["x.cpp", "/inc/a b.h", "c$d.h", "e.h"], self.guard.depfile_inputs(path))


if __name__ == "__main__":
    unittest.main()
