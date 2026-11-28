#!/usr/bin/env python3
"""Tests of the split of the libqore single compilation unit (cmake/QoreScuChunks.cmake, SINGLE_COMPILATION_UNIT_SPLIT,
qore#7780): the interpreter's call path and the rest of the wrapper, compiled one after the other.

- on a small wrapper: the files of the call path list go to the call path unit, all others to the rest, each in the
  order of the wrapper; a conditional group goes to the rest whole; "#undef ECHO" stays after the scanner; a file of
  the list that the wrapper does not include, a file of the list in a conditional, a character that breaks CMake list
  handling and an unsupported line are rejected; a second split with the same text does not rewrite the units (their
  mtimes, and so the objects, stay);
- on the real wrapper of CMakeLists.txt and the real list: every include in exactly one unit, every file of the list
  in the call path, and every source that defines a function of tools/stack-usage-budgets.txt (whose frames are on
  each level of a recursion of interpreted code) in the call path unit, so that the split keeps their inlining;
- qore_scu_compile_after() makes the object of the second unit depend on the object of the first in a toy project,
  with the Makefile generator and, when it is installed, the Ninja generator;
- the CI build jobs (test/docker_test/build.sh) configure the split.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
CHUNKS = ROOT / "cmake" / "QoreScuChunks.cmake"
BUDGETS = ROOT / "tools" / "stack-usage-budgets.txt"

WRAPPER = """// wrapper
#include "/b/parser.cpp"
#include "/b/scanner.cpp"
#undef ECHO
#include "/s/lib/A.cpp"
#include "/s/lib/B.cpp"
#ifdef HAVE_SIGNAL_HANDLING
#include "/s/lib/C.cpp"
#endif
#include "/s/lib/D.cpp"
#include "/s/lib/E.cpp"
"""


def flat(text):
    """the text with each run of whitespace collapsed to one space (CMake word-wraps the text of a message)"""
    return re.sub(r"\s+", " ", text)


def includes(text):
    return re.findall(r'^#include "([^"]+)"', text, re.M)


def real_wrapper():
    """the text of the single wrapper as CMakeLists.txt assembles it, with the directory variables substituted"""
    src = (ROOT / "CMakeLists.txt").read_text()
    m = re.search(r'set\(_scu_content\n"(.*?)"\)\n', src, re.S)
    assert m, "the wrapper assembly of CMakeLists.txt was not found"
    text = m.group(1).replace('\\"', '"')
    text = text.replace("${CMAKE_CURRENT_BINARY_DIR}", "/B").replace("${CMAKE_BINARY_DIR}", "/B")
    return text.replace("${CMAKE_SOURCE_DIR}", str(ROOT))


def call_path_files():
    """the files of QORE_SCU_CALL_PATH_FILES"""
    m = re.search(r"^set\(QORE_SCU_CALL_PATH_FILES\n(.*?)\)\n", CHUNKS.read_text(), re.S | re.M)
    assert m, "QORE_SCU_CALL_PATH_FILES was not found"
    return m.group(1).split()


class Split:
    """runs qore_scu_split_call_path() in cmake -P on a wrapper text, with the real list or one of its own"""
    def __init__(self, test, wrapper, files=None):
        self.tmp = Path(tempfile.mkdtemp(prefix="qore-scu-split-"))
        test.addCleanup(shutil.rmtree, str(self.tmp))
        (self.tmp / "wrapper.cpp").write_text(wrapper)
        files_line = "set(QORE_SCU_CALL_PATH_FILES %s)\n" % " ".join(files) if files is not None else ""
        (self.tmp / "run.cmake").write_text(
            'cmake_minimum_required(VERSION 3.14)\n'
            'include(${SRC}/cmake/QoreScuChunks.cmake)\n'
            + files_line +
            'file(READ ${WRAPPER} content)\n'
            'if (DEFINED RAW)\n set(content "${RAW}")\nendif ()\n'
            'string(REGEX REPLACE "\\n$" "" content "${content}")\n'
            'qore_scu_split_call_path("${content}" ${OUT}/unit- SRCS)\n'
            'message(STATUS "SRCS=${SRCS}")\n')

    def run(self, raw=None):
        out = self.tmp / "out"
        out.mkdir(exist_ok=True)
        cmd = ["cmake", "-DSRC=%s" % ROOT, "-DWRAPPER=%s" % (self.tmp / "wrapper.cpp"), "-DOUT=%s" % out]
        if raw is not None:
            cmd.append("-DRAW=%s" % raw)
        cmd += ["-P", str(self.tmp / "run.cmake")]
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        units = {}
        for name in ("call-path", "rest"):
            f = out / ("unit-%s.cpp" % name)
            if f.exists():
                units[name] = f.read_text()
        return p, units


class SmallWrapperTest(unittest.TestCase):
    def split(self, files=("A.cpp", "D.cpp")):
        return Split(self, WRAPPER, list(files))

    def test_partition_and_order(self):
        s = self.split()
        p, units = s.run()
        self.assertEqual(p.returncode, 0, p.stdout)
        self.assertEqual(["/s/lib/A.cpp", "/s/lib/D.cpp"], includes(units["call-path"]))
        self.assertEqual(["/b/parser.cpp", "/b/scanner.cpp", "/s/lib/B.cpp", "/s/lib/C.cpp", "/s/lib/E.cpp"],
                         includes(units["rest"]))
        # the call path first: the caller compiles the rest after it
        self.assertRegex(flat(p.stdout), r"SRCS=\S+/unit-call-path\.cpp;\S+/unit-rest\.cpp")

    def test_groups_and_undef(self):
        _, units = self.split().run()
        self.assertIn('scanner.cpp"\n#undef ECHO\n', units["rest"])
        self.assertIn('#ifdef HAVE_SIGNAL_HANDLING\n#include "/s/lib/C.cpp"\n#endif\n', units["rest"])
        self.assertNotIn("#if", units["call-path"])
        self.assertNotIn("#undef", units["call-path"])

    def test_undef_after_a_call_path_unit(self):
        wrapper = '#include "/s/lib/A.cpp"\n#undef X\n#include "/s/lib/B.cpp"\n'
        p, units = Split(self, wrapper, ["A.cpp"]).run()
        self.assertEqual(p.returncode, 0, p.stdout)
        self.assertIn('A.cpp"\n#undef X\n', units["call-path"])
        self.assertNotIn("#undef", units["rest"])

    def test_units_are_not_rewritten(self):
        s = self.split()
        s.run()
        out = s.tmp / "out"
        old = {f: (out / f).stat().st_mtime_ns for f in ("unit-call-path.cpp", "unit-rest.cpp")}
        # a later mtime would show a rewrite even on a file system with coarse timestamps
        for f in old:
            os.utime(out / f, ns=(old[f] - 10 ** 9, old[f] - 10 ** 9))
        p, _ = s.run()
        self.assertEqual(p.returncode, 0, p.stdout)
        for f in old:
            self.assertEqual(old[f] - 10 ** 9, (out / f).stat().st_mtime_ns, f)

    def test_rejects_bad_input(self):
        cases = (
            (["A.cpp", "Z.cpp"], None, "Z.cpp of QORE_SCU_CALL_PATH_FILES is not in the wrapper"),
            (["C.cpp"], None, "call path file C.cpp inside a conditional"),
            (["A.cpp"], '#include "/s/lib/A.cpp"\nint x', "unsupported wrapper line 'int x'"),
            (["A.cpp"], '#include "/s/lib/A[1].cpp"', "must not contain '['"),
            (["A.cpp"], '#include "/s/lib/A.cpp"\n#ifdef X\n#include "/s/lib/B.cpp"', "unterminated conditional"),
            (["A.cpp"], '#endif\n#include "/s/lib/A.cpp"', "unmatched '#endif'"),
        )
        for files, raw, error in cases:
            with self.subTest(error=error):
                p, _ = Split(self, WRAPPER, files).run(raw=raw)
                self.assertNotEqual(p.returncode, 0)
                self.assertIn(error, flat(p.stdout))


class RealWrapperTest(unittest.TestCase):
    def test_split_of_the_real_wrapper(self):
        wrapper = real_wrapper()
        self.assertNotIn("${", wrapper, "a variable of the wrapper assembly is not substituted by the test")
        p, units = Split(self, wrapper).run()
        self.assertEqual(p.returncode, 0, p.stdout)
        call, rest = includes(units["call-path"]), includes(units["rest"])
        self.assertEqual(sorted(call + rest), sorted(includes(wrapper)))
        self.assertEqual(len(call + rest), len(set(call + rest)))
        self.assertEqual(sorted(call_path_files()), sorted(Path(i).name for i in call))
        # both units are large: the split halves the memory of the compile only if neither has most of the code
        self.assertGreater(len(call), 20)
        self.assertGreater(len(rest), 300)

    def test_budgeted_functions_are_in_the_call_path(self):
        # each function of the stack-usage budgets is on each level of a recursion of interpreted code: the source
        # that defines it is in the call path unit, so that the split keeps the inlining of the single wrapper
        files = set(call_path_files())
        sources = {f.name: f.read_text(errors="replace") for f in (ROOT / "lib").glob("*.cpp")}
        missing = []
        for line in BUDGETS.read_text().splitlines():
            m = re.match(r"^\d+ (\S+)$", line)
            if not m:
                continue
            name = m.group(1)
            # a definition at the start of a line: the return type (if any) and the qualified name, then "(" or
            # template arguments
            definition = re.compile(r"^[A-Za-z_][^;\n]*?\b%s\s*[(<]" % re.escape(name), re.M)
            defined_in = sorted(f for f, text in sources.items() if definition.search(text))
            if not defined_in:
                # defined in a header (an inline function): compiled where it is used
                continue
            if not files.intersection(defined_in):
                missing.append("%s (%s)" % (name, ", ".join(defined_in)))
        self.assertEqual([], missing, "add their sources to QORE_SCU_CALL_PATH_FILES")


class CompileAfterTest(unittest.TestCase):
    def configure(self, generator):
        tmp = Path(tempfile.mkdtemp(prefix="qore-scu-after-"))
        self.addCleanup(shutil.rmtree, str(tmp))
        src = tmp / "src"
        src.mkdir()
        (src / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.14)\n"
            "project(scu_after CXX)\n"
            "include(%s)\n"
            "file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/lib/first.cpp \"int first() { return 1; }\\n\")\n"
            "file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/lib/second.cpp \"int second() { return 2; }\\n\")\n"
            "add_library(toy SHARED ${CMAKE_CURRENT_BINARY_DIR}/lib/first.cpp "
            "${CMAKE_CURRENT_BINARY_DIR}/lib/second.cpp)\n"
            "qore_scu_compile_after(toy ${CMAKE_CURRENT_BINARY_DIR}/lib/first.cpp "
            "${CMAKE_CURRENT_BINARY_DIR}/lib/second.cpp)\n" % CHUNKS)
        build = tmp / "build"
        p = subprocess.run(["cmake", "-G", generator, "-S", str(src), "-B", str(build)], stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, universal_newlines=True)
        self.assertEqual(0, p.returncode, p.stdout)
        return build

    def test_makefiles(self):
        build = self.configure("Unix Makefiles")
        rules = (build / "CMakeFiles/toy.dir/build.make").read_text()
        self.assertRegex(rules, r"(?m)^CMakeFiles/toy\.dir/lib/second\.cpp\.o: \S*CMakeFiles/toy\.dir/lib/first\.cpp\.o$")
        # the dependency is the object that the build makes: it builds, and the order is first, then second
        p = subprocess.run(["make", "-C", str(build), "-j2", "VERBOSE=1"], stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, universal_newlines=True)
        self.assertEqual(0, p.returncode, p.stdout)
        self.assertLess(p.stdout.index("first.cpp.o -c"), p.stdout.index("second.cpp.o -c"))

    def test_ninja(self):
        if not shutil.which("ninja"):
            self.skipTest("ninja is not installed")
        build = self.configure("Ninja")
        p = subprocess.run(["ninja", "-C", str(build), "-t", "query", "CMakeFiles/toy.dir/lib/second.cpp.o"],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        self.assertEqual(0, p.returncode, p.stdout)
        self.assertIn("CMakeFiles/toy.dir/lib/first.cpp.o", p.stdout)

    def test_first_outside_the_binary_directory(self):
        tmp = Path(tempfile.mkdtemp(prefix="qore-scu-after-"))
        self.addCleanup(shutil.rmtree, str(tmp))
        (tmp / "run.cmake").write_text(
            "include(%s)\nset(CMAKE_CURRENT_BINARY_DIR /b)\nqore_scu_compile_after(t /elsewhere/a.cpp /b/b.cpp)\n"
            % CHUNKS)
        p = subprocess.run(["cmake", "-P", str(tmp / "run.cmake")], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           universal_newlines=True)
        self.assertNotEqual(0, p.returncode)
        self.assertIn("/elsewhere/a.cpp is not in /b", flat(p.stdout))


class CiTest(unittest.TestCase):
    def test_build_jobs_configure_the_split(self):
        text = (ROOT / "test/docker_test/build.sh").read_text()
        self.assertRegex(text, r"-DSINGLE_COMPILATION_UNIT=1 -DSINGLE_COMPILATION_UNIT_SPLIT=ON\b")


if __name__ == "__main__":
    unittest.main()
