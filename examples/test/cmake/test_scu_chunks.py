#!/usr/bin/env python3
"""Tests of the libqore unity chunk split (cmake/QoreScuChunks.cmake, SINGLE_COMPILATION_UNIT_CHUNKS) and of the
tool that maintains its cost table (tools/update-scu-chunk-costs.py).

- on a small wrapper: every include lands in exactly one chunk (or is isolated), conditional groups stay whole, the
  order of the wrapper is kept in a chunk, the split is deterministic, the cost table (and the default cost of a file
  it does not list) drives the assignment, more chunks than units leave no empty chunk, a stale chunk file of an
  earlier larger N is removed, and a wrapper text with a character that breaks CMake list handling, an unsupported
  line or an unterminated conditional is rejected;
- on the real wrapper of CMakeLists.txt and the real cost table, for several N: every include exactly once, the
  #if/#endif pairs matched, "#undef ECHO" right after scanner.cpp, the order of the wrapper kept, the chunks balanced
  by cost, and the isolated files in no chunk;
- no source of the wrapper (or header) other than lib/QoreTermIOS.cpp and lib/QC_TermIOS.qpp includes <termios.h>,
  whose macros (CR1) collide with the LLVM headers (the reason why those two are isolated in a chunked build);
- the option SINGLE_COMPILATION_UNIT_CHUNKS rejects 0, a non-number and a leading zero at configure time;
- tools/update-scu-chunk-costs.py rounds to 10 ms, keeps the maximum of the duplicates of a name, skips malformed
  lines and files other than .cpp, and writes a sorted table.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import contextlib
import importlib.util
import io
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
COST_TABLE = ROOT / "cmake" / "scu-chunk-costs.txt"
TOOL = ROOT / "tools" / "update-scu-chunk-costs.py"
DEFAULT_COST = 2000
ISOLATED = ("QC_TermIOS.cpp", "QoreTermIOS.cpp")

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
#include "/b/QC_TermIOS.cpp"
#include "/s/lib/E.cpp"
#include "/s/lib/QoreTermIOS.cpp"
#include "/s/lib/F.cpp"
"""
COSTS = "# c\n9000 parser.cpp\n9000 scanner.cpp\n5000 A.cpp\n4000 B.cpp\n3000 C.cpp\n2000 D.cpp\n1000 E.cpp\n10 F.cpp\n"


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


def read_costs(path):
    costs = {}
    for line in path.read_text().splitlines():
        m = re.match(r"^(\d+) (\S+)$", line)
        if m:
            costs[m.group(2)] = int(m.group(1))
    return costs


class Split:
    def __init__(self, test, wrapper, costs):
        self.tmp = Path(tempfile.mkdtemp())
        test.addCleanup(shutil.rmtree, str(self.tmp))
        (self.tmp / "costs.txt").write_text(costs)
        (self.tmp / "wrapper.cpp").write_text(wrapper)
        (self.tmp / "run.cmake").write_text(
            'cmake_minimum_required(VERSION 3.14)\n'
            'include(${SRC}/cmake/QoreScuChunks.cmake)\n'
            'file(READ ${WRAPPER} content)\n'
            'if (DEFINED RAW)\n set(content "${RAW}")\nendif ()\n'
            'string(REGEX REPLACE "\\n$" "" content "${content}")\n'
            'qore_scu_split_chunks("${content}" ${N} ${COSTS} ${OUT}/chunk SRCS ISO)\n'
            'message(STATUS "ISO=${ISO}")\n')

    def run(self, n, out="out", raw=None):
        o = self.tmp / out
        o.mkdir(exist_ok=True)
        cmd = ["cmake", "-DSRC=%s" % ROOT, "-DWRAPPER=%s" % (self.tmp / "wrapper.cpp"), "-DN=%d" % n,
               "-DCOSTS=%s" % (self.tmp / "costs.txt"), "-DOUT=%s" % o]
        if raw is not None:
            cmd.append("-DRAW=%s" % raw)
        cmd += ["-P", str(self.tmp / "run.cmake")]
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        return p, {f.name: f.read_text() for f in sorted(o.glob("chunk*.cpp"))}


class SmallWrapperTest(unittest.TestCase):
    def split(self, wrapper=WRAPPER, costs=COSTS):
        return Split(self, wrapper, costs)

    def test_partition_and_isolation(self):
        p, chunks = self.split().run(2)
        self.assertEqual(p.returncode, 0, p.stdout)
        got = [i for t in chunks.values() for i in includes(t)]
        self.assertEqual(sorted(got), sorted(i for i in includes(WRAPPER) if "TermIOS" not in i))
        self.assertEqual(len(got), len(set(got)))
        self.assertIn("ISO=/b/QC_TermIOS.cpp;/s/lib/QoreTermIOS.cpp", p.stdout)

    def test_groups_undef_and_order(self):
        _, chunks = self.split().run(2)
        wrapper_order = includes(WRAPPER)
        for name, text in chunks.items():
            inc = includes(text)
            self.assertEqual(inc, [i for i in wrapper_order if i in inc], name)
            if any(i.endswith("/scanner.cpp") for i in inc):
                self.assertRegex(text, r'scanner\.cpp"\n#undef ECHO\n')
            if "/s/lib/C.cpp" in inc:
                self.assertRegex(text, r'#ifdef HAVE_SIGNAL_HANDLING\n#include "/s/lib/C\.cpp"\n#endif\n')

    def test_single_chunk_has_everything_in_order(self):
        _, chunks = self.split().run(1)
        self.assertEqual(list(chunks), ["chunk0.cpp"])
        self.assertEqual(includes(chunks["chunk0.cpp"]), [i for i in includes(WRAPPER) if "TermIOS" not in i])

    def test_deterministic(self):
        s = self.split()
        _, a = s.run(3, "o1")
        _, b = s.run(3, "o2")
        self.assertEqual(a, b)

    def test_costs_drive_assignment(self):
        # the two heaviest units (F.cpp and scanner.cpp, which sort last by name) end in different chunks, and the
        # three cheap units that sort first by name follow the load, not the name order (A, B, C would alternate)
        costs = "9000 F.cpp\n9000 scanner.cpp\n1 parser.cpp\n1 A.cpp\n1 B.cpp\n1 C.cpp\n1 D.cpp\n1 E.cpp\n"
        _, chunks = self.split(costs=costs).run(2)
        where = {i: n for n, t in chunks.items() for i in includes(t)}
        self.assertNotEqual(where["/s/lib/F.cpp"], where["/b/scanner.cpp"])
        self.assertEqual(sorted(len(includes(t)) for t in chunks.values()), [4, 4])
        # swapping the costs moves the heavy units: parser.cpp and A.cpp heavy now
        costs = "9000 parser.cpp\n9000 A.cpp\n1 scanner.cpp\n1 B.cpp\n1 C.cpp\n1 D.cpp\n1 E.cpp\n1 F.cpp\n"
        _, chunks = self.split(costs=costs).run(2)
        where = {i: n for n, t in chunks.items() for i in includes(t)}
        self.assertNotEqual(where["/b/parser.cpp"], where["/s/lib/A.cpp"])

    def test_default_cost_for_unlisted_files(self):
        # F is listed as nearly free; every other file is unlisted and costs the default, so with the isolated
        # files gone the 7 default-cost units split 4/3 and the cheap F goes to the lighter chunk
        _, chunks = self.split(costs="1 F.cpp\n").run(2)
        counts = sorted(len(includes(t)) for t in chunks.values())
        self.assertEqual(counts, [4, 4])
        where = {i: n for n, t in chunks.items() for i in includes(t)}
        # equal costs are ordered by name (ASCII: A, B, C, D, E, parser, scanner) and alternate between the chunks:
        # A, C, E and scanner go to chunk 0 (load 8000), B, D and parser to chunk 1 (6000), and the cheap F to chunk 1
        self.assertNotEqual(where["/b/parser.cpp"], where["/s/lib/A.cpp"])
        self.assertEqual(where["/b/parser.cpp"], where["/s/lib/B.cpp"])
        self.assertEqual(where["/s/lib/F.cpp"], where["/s/lib/B.cpp"])
        self.assertEqual(where["/b/scanner.cpp"], where["/s/lib/A.cpp"])

    def test_more_chunks_than_units(self):
        _, chunks = self.split().run(50)
        self.assertEqual(sum(len(includes(t)) for t in chunks.values()), 8)
        self.assertTrue(all(includes(t) for t in chunks.values()))
        self.assertEqual(len(chunks), 8)

    def test_stale_chunks_of_a_larger_n_are_removed(self):
        s = self.split()
        _, chunks = s.run(6, "same")
        self.assertEqual(len(chunks), 6)
        _, chunks = s.run(2, "same")
        self.assertEqual(sorted(chunks), ["chunk0.cpp", "chunk1.cpp"])
        self.assertEqual(sorted(f.name for f in (s.tmp / "same").glob("chunk*")),
                         ["chunk0.cpp", "chunk0.cpp.in", "chunk1.cpp", "chunk1.cpp.in"])

    def test_rejects_bad_input(self):
        for raw, message in (
                ('#include "/s/lib/A.cpp"\n#include "/s/lib/B.cpp";', "must not contain ';'"),
                ('#include "/s/li[b/A.cpp"', "must not contain '['"),
                ('#include "/s/li]b/A.cpp"', "must not contain ']'"),
                ('#include "/s/li\\b/A.cpp"', "must not contain '\\'"),
                ('#include "/s/lib/A.cpp"\nint x', "unsupported wrapper line"),
                ('#ifdef X\n#include "/s/lib/A.cpp"', "unterminated conditional"),
                ('#endif', "unmatched '#endif'"),
                ('#ifdef X\n#ifdef Y\n#include "/s/lib/A.cpp"\n#endif\n#endif', "nested conditional")):
            p, chunks = self.split().run(2, "bad", raw)
            self.assertNotEqual(p.returncode, 0, raw)
            self.assertIn(message, p.stdout, raw)
            self.assertEqual(chunks, {}, raw)

    def test_rejects_bad_chunk_count(self):
        for n in (0, 257):
            p, chunks = self.split().run(n, "badn")
            self.assertNotEqual(p.returncode, 0, n)
            self.assertIn("chunk count must be between 1 and 256", p.stdout)
            self.assertEqual(chunks, {})

    def test_malformed_cost_line_warns(self):
        p, chunks = self.split(costs=COSTS + "12\tF.cpp\nnot a cost line\n").run(2)
        self.assertEqual(p.returncode, 0, p.stdout)
        self.assertIn("ignoring malformed line '12\tF.cpp'", p.stdout)
        self.assertIn("ignoring malformed line 'not a cost line'", p.stdout)
        self.assertEqual(len(chunks), 2)


class RealWrapperTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.wrapper = real_wrapper()
        cls.costs = read_costs(COST_TABLE)

    def cost(self, include):
        return self.costs.get(Path(include).name, DEFAULT_COST)

    def test_chunks_of_the_real_wrapper(self):
        self.assertNotIn("${", self.wrapper, "a variable of the wrapper assembly is not substituted by the test")
        all_includes = includes(self.wrapper)
        names = [Path(i).name for i in all_includes]
        self.assertEqual(len(names), len(set(names)), "the cost table is keyed by base name, which must be unique")
        self.assertGreater(len(all_includes), 400)
        self.assertEqual(len(all_includes), len(set(all_includes)))
        kept = [i for i in all_includes if Path(i).name not in ISOLATED]
        self.assertEqual(len(kept), len(all_includes) - 2)
        for n in (1, 2, 3, 4, 7, 16):
            with self.subTest(n=n):
                s = Split(self, self.wrapper, COST_TABLE.read_text())
                p, chunks = s.run(n)
                self.assertEqual(p.returncode, 0, p.stdout)
                self.assertEqual(len(chunks), n)
                got = [i for t in chunks.values() for i in includes(t)]
                self.assertEqual(sorted(got), sorted(kept))
                for name, text in chunks.items():
                    inc = includes(text)
                    self.assertEqual(inc, [i for i in kept if i in set(inc)], name)
                    self.assertEqual(len(re.findall(r"^#if", text, re.M)), len(re.findall(r"^#endif", text, re.M)))
                    if any(i.endswith("/scanner.cpp") for i in inc):
                        self.assertIn('scanner.cpp"\n#undef ECHO\n', text)
                    else:
                        self.assertNotIn("#undef ECHO", text)
                # every #if/#endif of the wrapper is in exactly one chunk, unbroken
                self.assertEqual(sum(len(re.findall(r"^#if", t, re.M)) for t in chunks.values()),
                                 len(re.findall(r"^#if", self.wrapper, re.M)))
                for t in chunks.values():
                    for blk in re.findall(r"^#if[a-z]* [^\n]+\n(.*?)^#endif\n", t, re.M | re.S):
                        self.assertNotIn("#if", blk)
                for iso in ISOLATED:
                    self.assertFalse(any(iso in i for i in got), iso)
                if n > 1:
                    loads = [sum(self.cost(i) for i in includes(t)) for t in chunks.values()]
                    # longest-processing-time-first: the spread is at most the heaviest unit
                    self.assertLessEqual(max(loads) - min(loads), max(self.cost(i) for i in kept))
                    if n <= 4:
                        self.assertLess(max(loads) / min(loads), 1.02, loads)
                _, again = Split(self, self.wrapper, COST_TABLE.read_text()).run(n)
                self.assertEqual(again, chunks)

    def test_termios_header_stays_out_of_the_other_sources(self):
        # <termios.h> defines CR1, ECHO, ...; the LLVM headers use such names as identifiers.  Only these two
        # sources may include it (they are isolated in a chunked build); any other one could silently bring the
        # collision back in some chunk assignment.
        allowed = {"lib/QoreTermIOS.cpp", "lib/QC_TermIOS.qpp"}
        found = set()
        pattern = re.compile(r"^\s*#\s*include\s*[<\"](?:sys/)?termios\.h[>\"]", re.M)
        for d, pats in (("lib", ("*.cpp", "*.qpp", "*.h")), ("include", ("*.h", "*.hpp"))):
            for pat in pats:
                for f in (ROOT / d).rglob(pat):
                    if pattern.search(f.read_text(errors="replace")):
                        found.add(str(f.relative_to(ROOT)))
        self.assertEqual(found, allowed)
        for iso in ISOLATED:
            self.assertIn(iso, self.wrapper)


class OptionValidationTest(unittest.TestCase):
    def test_invalid_values_are_rejected(self):
        for value in ("0", "abc", "01", "-2", "1.5", "", "257", "1000", "99999999999999999999"):
            with self.subTest(value=value):
                tmp = tempfile.mkdtemp()
                self.addCleanup(shutil.rmtree, tmp)
                p = subprocess.run(["cmake", "-S", str(ROOT), "-B", tmp, "-DSINGLE_COMPILATION_UNIT_CHUNKS=" + value],
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
                self.assertNotEqual(p.returncode, 0)
                self.assertIn("SINGLE_COMPILATION_UNIT_CHUNKS must be a positive integer", p.stdout)


def load_tool():
    spec = importlib.util.spec_from_file_location("update_scu_chunk_costs", str(TOOL))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class CostToolTest(unittest.TestCase):
    def run_tool(self, log):
        tmp = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, str(tmp))
        (tmp / "log").write_text(log)
        rc = load_tool().main(["tool", str(tmp / "log"), str(tmp / "out")])
        return rc, (tmp / "out").read_text().splitlines()

    def test_rounding_duplicates_and_malformed_lines(self):
        rc, lines = self.run_tool(
            "12.34 1000 /s/lib/B.cpp\n"
            "2.5 1000 /s/lib/A.cpp\n"
            "7.0 1000 /x/build-a/B.cpp\n"      # same name: the maximum (12.34 above) is kept
            "3.0 1000 /s/lib/B.cpp\n"
            "nan-ish 1000 /s/lib/Z.cpp\n"      # not a number
            "5.0 1000\n"                       # too few fields
            "\n"
            "9.9 1000 /s/lib/foo.c\n"          # not a .cpp file
            "0.0 1000 /s/lib/Zero.cpp\n")
        self.assertEqual(rc, 0)
        body = [l for l in lines if not l.startswith("#")]
        self.assertEqual(body, ["2500 A.cpp", "12340 B.cpp", "0 Zero.cpp"])
        self.assertTrue(lines[0].startswith("# Copyright 2026"))

    def test_rounds_to_ten_milliseconds(self):
        _, lines = self.run_tool("1.234 1 /s/a.cpp\n1.236 1 /s/b.cpp\n")
        self.assertEqual([l for l in lines if not l.startswith("#")], ["1230 a.cpp", "1240 b.cpp"])

    def test_usage_error(self):
        with contextlib.redirect_stderr(io.StringIO()) as err:
            self.assertEqual(load_tool().main(["tool"]), 2)
        self.assertIn("usage: tools/update-scu-chunk-costs.py", err.getvalue())


if __name__ == "__main__":
    unittest.main()
