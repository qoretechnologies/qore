#!/usr/bin/env python3
"""Tests of the stack usage check (tools/check-stack-usage.py, tools/stack-usage-budgets.txt; issue #5852).

- the budget file of the repository parses, names each function once and is calibrated for a compiler and build
  type;
- .su records of GCC and Clang are parsed, and a budget matches a function's own frame only: not a lambda or local
  class inside it, not a function of another class whose name ends with the same name, and every clone of it;
- a function split by partial inlining counts its frame and that of its .part clone, which GCC records as
  "0(<params>)" at the function's location (also with the real g++ when it is available); other records of the
  function (GCC's .constprop and .isra clones) count only the largest;
- a frame over its budget and a function with no frame fail the check, frames within budget pass it, a build made
  with another compiler or major version, build type, flags or target (C library) is skipped unless --force is
  given, and a build directory without the configuration that QORE_STACK_USAGE_REPORT writes, or without .su files,
  is an input error.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import contextlib
import importlib.util
import io
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
TOOL = ROOT / "tools" / "check-stack-usage.py"
BUDGETS = ROOT / "tools" / "stack-usage-budgets.txt"


def load_tool():
    spec = importlib.util.spec_from_file_location("check_stack_usage", str(TOOL))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


csu = load_tool()

GCC_SU = (
    "/src/lib/Function.cpp:10:10:QoreValue UserVariantBase::eval(const char*, CodeEvaluationHelper*, QoreObject*, "
    "ExceptionSink*, const qore_class_private*, bool) const\t512\tstatic\n"
    "/src/lib/QoreClass.cpp:20:11:QoreValue NormalMethodFunction::evalMethodTmpArgs(ExceptionSink*, RuntimeConfig&, "
    "const AbstractQoreFunctionVariant*, QoreObject*, QoreListNode*, const qore_class_private*, "
    "const QoreTypeParamInstantiation*, const AbstractQoreFunctionVariant**) const\t896\tstatic\n"
    # a lambda inside the function is not the function
    "/src/lib/QoreClass.cpp:30:5:NormalMethodFunction::evalMethodTmpArgs(ExceptionSink*, RuntimeConfig&) const::"
    "<lambda()>\t4000\tstatic\n"
    # nor is a function of a class whose name ends with the same name
    "/src/lib/QoreClass.cpp:40:5:QoreValue XNormalMethodFunction::evalMethodTmpArgs(int)\t5000\tstatic\n"
    # nor one of a nested class with the same name
    "/src/lib/QoreClass.cpp:45:5:QoreValue Outer::NormalMethodFunction::evalMethodTmpArgs(int)\t5000\tstatic\n"
    # GCC records a .constprop or .isra clone of the function under the function's own name
    "/src/lib/Function.cpp:10:10:QoreValue UserVariantBase::eval(const char*, CodeEvaluationHelper*, QoreObject*, "
    "ExceptionSink*, const qore_class_private*, bool) const\t600\tdynamic,bounded\n"
    "this line is not a frame record\n"
)

CLANG_SU = "/src/lib/Function.cpp:10:10:UserVariantBase::eval\t512\tstatic\n"


@contextlib.contextmanager
def build_dir(su_text, compiler="GNU", build_type="Release", write_config=True, target="x86_64-linux-gnu",
        version="15.2.0", build_type_flags="-O3 -DNDEBUG", cxx_flags="-fno-strict-aliasing -std=c++20",
        hardening="_FORTIFY_SOURCE=3 __CET__=3 __SSP_STRONG__=3", ipo="0"):
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        if write_config:
            (tmp / "stack-usage-config.txt").write_text(
                "# test\ncompiler=%s\ncompiler-version=%s\nbuild-type=%s\nbuild-type-flags=%s\ncxx-flags=%s\n"
                "hardening=%s\nipo=%s\ntarget=%s\n"
                % (compiler, version, build_type, build_type_flags, cxx_flags, hardening, ipo, target))
        if su_text is not None:
            obj = tmp / "CMakeFiles" / "libqore.dir" / "lib"
            obj.mkdir(parents=True)
            (obj / "single-compilation-unit.cpp.su").write_text(su_text)
        yield tmp


def frames_of(su_text, path=None):
    """Returns the frame records of .su text as collect_frames() returns them."""
    frames = []
    for line in su_text.splitlines():
        rec = csu.parse_su_line(line)
        if rec:
            frames.append((rec[0], rec[1], path, rec[2]))
    return frames


def run(argv):
    out = io.StringIO()
    err = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        rc = csu.main(argv)
    return rc, out.getvalue(), err.getvalue()


def write_budgets(tmp, body,
        settings="@compiler GNU\n@build-type Release\n@compiler-version 15.2.0\n@target x86_64-linux-gnu\n"):
    path = Path(tmp) / "budgets.txt"
    path.write_text(settings + body)
    return str(path)


class RepositoryBudgetsTest(unittest.TestCase):
    def test_budget_file_parses(self):
        settings, budgets = csu.parse_budgets(BUDGETS.read_text(), str(BUDGETS))
        self.assertEqual("GNU", settings["compiler"])
        self.assertEqual("release", settings["build-type"].lower())
        self.assertEqual("x86_64-linux-gnu", settings["target"])
        names = [b[0] for b in budgets]
        # the frames of the per-call path of interpreted code that issue #5852 measured
        for func in ("QoreIRInterpreter::execute", "NormalMethodFunction::evalMethodTmpArgs",
                "StaticMethodFunction::evalMethodTmpArgs", "UserVariantBase::eval"):
            self.assertIn(func, names)
        for _, size, _ in budgets:
            self.assertGreater(size, 0)


class ParseTest(unittest.TestCase):
    def test_su_lines(self):
        sig, size, location = csu.parse_su_line(GCC_SU.splitlines()[0])
        self.assertEqual(512, size)
        self.assertEqual("/src/lib/Function.cpp:10:10", location)
        self.assertTrue(sig.startswith("QoreValue UserVariantBase::eval(const char*"))
        self.assertEqual(("UserVariantBase::eval", 512, "/src/lib/Function.cpp:10:10"),
            csu.parse_su_line(CLANG_SU.strip()))
        self.assertIsNone(csu.parse_su_line("this line is not a frame record"))
        self.assertIsNone(csu.parse_su_line("a:1:1:f()\tbig\tstatic"))

    def test_matching(self):
        sigs = [csu.parse_su_line(l)[0] for l in GCC_SU.splitlines() if csu.parse_su_line(l)]
        hits = [s for s in sigs if csu.matches("NormalMethodFunction::evalMethodTmpArgs", s)]
        self.assertEqual(1, len(hits))
        self.assertIn("const AbstractQoreFunctionVariant**) const", hits[0])
        hits = [s for s in sigs if csu.matches("UserVariantBase::eval", s)]
        self.assertEqual(2, len(hits))
        # a Clang record names the function without its parameters
        self.assertTrue(csu.matches("UserVariantBase::eval", "UserVariantBase::eval"))
        self.assertFalse(csu.matches("UserVariantBase::eval", "UserVariantBase::evalIntern(int)"))

    def test_budget_errors(self):
        good = "@compiler GNU\n@build-type Release\n@target x86_64-linux-gnu\n"
        for bad in ("@compiler\n@build-type Release\n@target t\n100 f\n", good + "big f\n", good + "100 f g\n",
                good + "100 f(int)\n", good + "100 f\n200 f\n", "@build-type Release\n@target t\n100 f\n",
                "@compiler GNU\n@build-type Release\n100 f\n"):
            with self.assertRaises(csu.InputError, msg=bad):
                csu.parse_budgets(bad)
        settings, budgets = csu.parse_budgets(good + "# comment\n\n100 A::f\n")
        self.assertEqual([("A::f", 100, 6)], budgets)

    def test_largest_record_counts(self):
        frames = frames_of(GCC_SU)
        results = csu.check([("UserVariantBase::eval", 700, 1)], frames)
        self.assertEqual(600, results[0][2])


class CheckTest(unittest.TestCase):
    def test_within_budget(self):
        with build_dir(GCC_SU) as d:
            b = write_budgets(d, "600 UserVariantBase::eval\n896 NormalMethodFunction::evalMethodTmpArgs\n")
            rc, out, _ = run(["--budgets", b, str(d)])
        self.assertEqual(0, rc, out)
        self.assertIn("OK: 2 function(s) within budget", out)

    def test_over_budget(self):
        with build_dir(GCC_SU) as d:
            b = write_budgets(d, "599 UserVariantBase::eval\n896 NormalMethodFunction::evalMethodTmpArgs\n")
            rc, out, _ = run(["--budgets", b, str(d)])
        self.assertEqual(1, rc, out)
        self.assertRegex(out, r"OVER\s+UserVariantBase::eval\s+600 B\s+budget\s+599 B\s+\(\+1\)")
        self.assertIn("FAILED: 1 function(s)", out)

    def test_missing(self):
        with build_dir(GCC_SU) as d:
            b = write_budgets(d, "600 UserVariantBase::eval\n100 QoreIRInterpreter::execute\n")
            rc, out, _ = run(["--budgets", b, str(d)])
        self.assertEqual(1, rc, out)
        self.assertRegex(out, r"MISSING\s+QoreIRInterpreter::execute")

    def test_other_build_skipped(self):
        for compiler, build_type, target in (("Clang", "Release", "x86_64-linux-gnu"),
                ("GNU", "Debug", "x86_64-linux-gnu"), ("GNU", "Release", "x86_64-alpine-linux-musl")):
            with build_dir(GCC_SU, compiler, build_type, target=target) as d:
                b = write_budgets(d, "1 UserVariantBase::eval\n")
                rc, out, _ = run(["--budgets", b, str(d)])
                self.assertEqual(0, rc, out)
                self.assertIn("SKIPPED", out)
                # --force checks it anyway
                rc, out, _ = run(["--budgets", b, "--force", str(d)])
                self.assertEqual(1, rc, out)

    def test_compiler_version(self):
        # another major version is skipped unless --force is given
        with build_dir(GCC_SU) as d:
            b = write_budgets(d, "1 UserVariantBase::eval\n",
                "@compiler GNU\n@build-type Release\n@compiler-version 14.1.0\n@target x86_64-linux-gnu\n")
            rc, out, _ = run(["--budgets", b, str(d)])
            self.assertEqual(0, rc, out)
            self.assertIn("SKIPPED", out)
            self.assertIn("compiler version 15.2.0, budgets for major version 14", out)
            rc, out, _ = run(["--budgets", b, "--force", str(d)])
            self.assertEqual(1, rc, out)
        # another minor version of the same major version is checked
        with build_dir(GCC_SU, version="15.1.0") as d:
            b = write_budgets(d, "1 UserVariantBase::eval\n")
            rc, out, _ = run(["--budgets", b, str(d)])
            self.assertEqual(1, rc, out)

    def test_flags(self):
        settings = ("@compiler GNU\n@build-type Release\n@compiler-version 15.2.0\n@target x86_64-linux-gnu\n"
            "@build-type-flags -O3 -DNDEBUG\n@cxx-flags -fno-strict-aliasing -std=c++20\n"
            "@hardening _FORTIFY_SOURCE=3 __CET__=3 __SSP_STRONG__=3\n@ipo 0\n")
        # the same flags in another order are the calibrated build, which is checked
        with build_dir(GCC_SU, build_type_flags="-DNDEBUG -O3", cxx_flags="-std=c++20 -fno-strict-aliasing") as d:
            b = write_budgets(d, "1 UserVariantBase::eval\n", settings)
            rc, out, _ = run(["--budgets", b, str(d)])
            self.assertEqual(1, rc, out)
        # other flags are not: link-time optimization, hardening flags, another optimization level
        for kwargs, text in (({"cxx_flags": "-fno-strict-aliasing -std=c++20 -flto"}, "cxx-flags"),
                ({"cxx_flags": "-O2 -fstack-protector-strong -fno-strict-aliasing -std=c++20"}, "cxx-flags"),
                ({"build_type_flags": "-O2 -DNDEBUG"}, "build-type-flags"),
                # another distribution's GCC with other hardening defaults (Alpine's), link-time optimization, and a
                # multi-config generator, which has no build type at configure time
                ({"hardening": "_FORTIFY_SOURCE=2 __SSP_STRONG__=3"}, "hardening"),
                ({"ipo": "1"}, "link-time optimization"),
                ({"build_type": "multi-config", "build_type_flags": ""}, "build type multi-config")):
            with build_dir(GCC_SU, **kwargs) as d:
                b = write_budgets(d, "1 UserVariantBase::eval\n", settings)
                rc, out, _ = run(["--budgets", b, str(d)])
                self.assertEqual(0, rc, out)
                self.assertIn("SKIPPED", out)
                self.assertIn(text, out)

    def test_part_clone_adds(self):
        # GCC records the .part clone of a function split by partial inlining as "0(<params>)" at the function's own
        # location; both frames can be on the stack, as the function calls the clone
        su = ("/src/p.cpp:4:5:0(int)\t576\tstatic\n"
            "/src/p.cpp:4:5:int big(int)\t8\tstatic\n"
            "/src/p.cpp:11:5:int c1(int)\t16\tstatic\n"
            # the clone of another function at another location is not big()'s
            "/src/p.cpp:20:5:0(int)\t900\tstatic\n"
            "/src/p.cpp:20:5:int other(int)\t8\tstatic\n")
        frames = frames_of(su)
        self.assertTrue(csu.is_part_clone("0(int)"))
        self.assertFalse(csu.is_part_clone("int big(int)"))
        results = csu.check([("big", 600, 1), ("c1", 100, 2)], frames)
        self.assertEqual(584, results[0][2])
        self.assertIn(".part clone 0(int) at /src/p.cpp:4:5", results[0][3])
        self.assertEqual(16, results[1][2])
        # the same location in another .su file is another function
        frames2 = [(f[0], f[1], Path("other.su") if f[0] == "0(int)" else f[2], f[3]) for f in frames]
        self.assertEqual(8, csu.check([("big", 600, 1)], frames2)[0][2])

    @unittest.skipUnless(shutil.which("g++"), "g++ is not available")
    def test_part_clone_with_gcc(self):
        # a function that GCC splits by partial inlining at -O3: the cold part with the large arrays becomes a .part
        # clone that the callers call after the inlined head
        source = """#include <cstring>
extern void use(char*);
extern int flag;
int big(int x) {
    if (flag) return 1;
    char buf[300]; memset(buf, x, sizeof buf); use(buf); use(buf+1); use(buf+2); use(buf+3);
    for (int i = 0; i < x; ++i) { char b2[200]; memset(b2, i, sizeof b2); use(b2); use(b2+1); use(b2+2);}
    use(buf+4); use(buf+5); use(buf+6); use(buf+7); use(buf+8);
    return buf[5];
}
int c1(int n) { return big(n) + 1; }
int c2(int n) { return big(n*2) + 2; }
"""
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            (tmp / "p.cpp").write_text(source)
            subprocess.run(["g++", "-O3", "-fstack-usage", "-c", "p.cpp", "-o", "p.o"], cwd=str(tmp), check=True)
            frames = csu.collect_frames(tmp)
        parts = [f for f in frames if csu.is_part_clone(f[0])]
        if not parts:
            self.skipTest("this g++ does not split big() by partial inlining")
        heads = [f for f in frames if csu.matches("big", f[0])]
        self.assertEqual(1, len(heads))
        self.assertEqual(heads[0][3], parts[0][3])
        self.assertGreater(parts[0][1], 300)
        results = csu.check([("big", 1, 1)], frames)
        self.assertEqual(heads[0][1] + parts[0][1], results[0][2])


if __name__ == "__main__":
    unittest.main()
