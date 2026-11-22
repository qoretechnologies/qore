# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Tests of tools/check-hot-inlining.py: the out-of-line calls of x86_64 and aarch64 code are recognized, and the check
fails for a library of an architecture whose calls it does not recognize (qore#7165)"""

import contextlib
import importlib.util
import io
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SCRIPT = os.path.join(ROOT, "tools", "check-hot-inlining.py")


def load_checker():
    spec = importlib.util.spec_from_file_location("check_hot_inlining", SCRIPT)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


checker = load_checker()

# objdump -d -C --no-show-raw-insn of the code of a caller, as GNU binutils prints it for each architecture
X86_64_CODE = """
0000000000012340 <caller(int)>:
   12340:\tpush   %rbx
   12341:\tcall   12400 <Foo::bar(int)>
   12346:\tcall   *%rax
   12348:\tcall   11000 <Foo::bar(int)@plt>
   1234d:\tjmp    12360 <caller(int)+0x20>
   1234f:\tcall   12500 <Foo::baz(int) [clone .cold]>
   12354:\tjmp    12600 <Foo::qux()>
"""

AARCH64_CODE = """
0000000000012340 <caller(int)>:
   12340:\tstp\tx29, x30, [sp, #-32]!
   12344:\tbl\t12400 <Foo::bar(int)>
   12348:\tblr\tx8
   1234c:\tbl\t11000 <Foo::bar(int)@plt>
   12350:\tb.ne\t12360 <caller(int)+0x20>
   12354:\tcbz\tw0, 12370 <caller(int)+0x30>
   12358:\tbl\t12500 <Foo::baz(int) [clone .cold]>
   1235c:\tb\t12600 <Foo::qux()>
   12360:\tbr\tx16
"""


class CallRecognitionTest(unittest.TestCase):
    """the calls of each architecture are counted from the disassembly"""

    def test_x86_64(self):
        found = checker.count_calls(X86_64_CODE, checker.CALL_PATTERNS["x86-64"])
        # the direct calls (also through the PLT) and the tail jump; not the indirect call or the .cold fragment
        self.assertEqual(2, found.get("Foo::bar"))
        self.assertEqual(1, found.get("Foo::qux"))
        self.assertNotIn("Foo::baz", found)

    def test_aarch64(self):
        found = checker.count_calls(AARCH64_CODE, checker.CALL_PATTERNS["aarch64"])
        # bl (also through the PLT) and the tail branch b; not blr, br, b.<cond>, cbz or the .cold fragment
        self.assertEqual(2, found.get("Foo::bar"))
        self.assertEqual(1, found.get("Foo::qux"))
        self.assertNotIn("Foo::baz", found)
        # the branches within the function name the caller itself: only the tail branch's target is another function
        self.assertNotIn("caller", found)

    def test_patterns_of_architectures(self):
        # one architecture's calls are not another's: x86_64 code has no bl, aarch64 code no call
        self.assertEqual({}, checker.count_calls(AARCH64_CODE, checker.CALL_PATTERNS["x86-64"]))
        self.assertEqual({}, checker.count_calls(X86_64_CODE, checker.CALL_PATTERNS["aarch64"]))


class UnknownArchitectureTest(unittest.TestCase):
    """a library of an architecture whose calls are not recognized fails the check instead of passing it"""

    def test_unknown_architecture_fails(self):
        with tempfile.TemporaryDirectory() as d:
            lib = os.path.join(d, "libqore.so.1")
            with open(lib, "w") as f:
                f.write("")
            saved = (checker.compiler_of, checker.library_arch, sys.argv)
            checker.compiler_of = lambda lib: "GCC: (test) 13"
            checker.library_arch = lambda lib: "riscv:rv64"
            sys.argv = ["check-hot-inlining.py", d]
            out = io.StringIO()
            try:
                with contextlib.redirect_stdout(out):
                    rc = checker.main()
            finally:
                checker.compiler_of, checker.library_arch, sys.argv = saved
            self.assertEqual(2, rc, out.getvalue())
            self.assertIn("riscv:rv64", out.getvalue())
            self.assertIn("cannot be checked", out.getvalue())


SOURCE = r"""
extern "C++" {
__attribute__((noinline)) int hotCallee(int x) { return x * 3 + 1; }
int hotCaller(int x) { return hotCallee(x) + 2; }
static inline __attribute__((always_inline)) int inlinedCallee(int x) { return x * 5 + 7; }
int inlinedCaller(int x) { return inlinedCallee(x) - 1; }
}
"""


@unittest.skipUnless(all(shutil.which(t) for t in ("g++", "objdump", "nm", "readelf")),
                     "needs GCC and GNU binutils")
class HostBuildTest(unittest.TestCase):
    """the check of a library that GCC made for this machine (x86_64 or aarch64): an out-of-line call is found"""

    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.mkdtemp(prefix="check-hot-inlining-")
        # removed also if the build fails (tearDownClass() does not run then)
        cls.addClassCleanup(shutil.rmtree, cls.dir)
        src = os.path.join(cls.dir, "lib.cpp")
        with open(src, "w") as f:
            f.write(SOURCE)
        cls.lib = os.path.join(cls.dir, "libqore.so.1")
        subprocess.run(["g++", "-O2", "-shared", "-fPIC", "-o", cls.lib, src], check=True)
        cls.arch = checker.library_arch(cls.lib)

    def run_check(self, lines):
        lst = os.path.join(self.dir, "list.txt")
        with open(lst, "w") as f:
            f.write("\n".join(lines) + "\n")
        p = subprocess.run([sys.executable, "-I", SCRIPT, self.dir, "--list", lst], capture_output=True, text=True)
        return p.returncode, p.stdout + p.stderr

    def test_out_of_line_call_is_found(self):
        if self.arch not in checker.CALL_PATTERNS:
            self.skipTest("the calls of %r are not recognized" % self.arch)
        rc, out = self.run_check(["hotCaller hotCallee"])
        self.assertEqual(1, rc, out)
        self.assertIn("NOT-INLINED hotCaller calls hotCallee out of line 1 time(s)", out)

    def test_allowed_calls(self):
        if self.arch not in checker.CALL_PATTERNS:
            self.skipTest("the calls of %r are not recognized" % self.arch)
        rc, out = self.run_check(["hotCaller hotCallee 1"])
        self.assertEqual(0, rc, out)

    def test_inlined_call_passes(self):
        if self.arch not in checker.CALL_PATTERNS:
            self.skipTest("the calls of %r are not recognized" % self.arch)
        rc, out = self.run_check(["inlinedCaller inlinedCallee"])
        self.assertEqual(0, rc, out)
        self.assertIn("all 1 hot inlines", out)


if __name__ == "__main__":
    unittest.main()
