#!/usr/bin/env python3
"""Tests of the stack usage check (tools/check-stack-usage.py, tools/stack-usage-budgets.txt; issue #5852).

- the budget file of the repository parses, names each function once and is calibrated for a compiler and build
  type;
- .su records of GCC and Clang are parsed, and a budget matches a function's own frame only: not a lambda or local
  class inside it, not a function of another class whose name ends with the same name, and every clone of it;
- a function split into a head and a .part clone counts both frames, other clones only the largest;
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
    # a clone of the function is the function
    "/src/lib/Function.cpp:50:5:QoreValue UserVariantBase::eval(const char*) const [clone .constprop.0]\t600\t"
    "dynamic,bounded\n"
    "this line is not a frame record\n"
)

CLANG_SU = "/src/lib/Function.cpp:10:10:UserVariantBase::eval\t512\tstatic\n"


@contextlib.contextmanager
def build_dir(su_text, compiler="GNU", build_type="Release", write_config=True, target="x86_64-linux-gnu",
        version="15.2.0", build_type_flags="-O3 -DNDEBUG", cxx_flags="-fno-strict-aliasing -std=c++20"):
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        if write_config:
            (tmp / "stack-usage-config.txt").write_text(
                "# test\ncompiler=%s\ncompiler-version=%s\nbuild-type=%s\nbuild-type-flags=%s\ncxx-flags=%s\n"
                "target=%s\n" % (compiler, version, build_type, build_type_flags, cxx_flags, target))
        if su_text is not None:
            obj = tmp / "CMakeFiles" / "libqore.dir" / "lib"
            obj.mkdir(parents=True)
            (obj / "single-compilation-unit.cpp.su").write_text(su_text)
        yield tmp


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
        sig, size = csu.parse_su_line(GCC_SU.splitlines()[0])
        self.assertEqual(512, size)
        self.assertTrue(sig.startswith("QoreValue UserVariantBase::eval(const char*"))
        self.assertEqual(("UserVariantBase::eval", 512), csu.parse_su_line(CLANG_SU.strip()))
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

    def test_largest_clone_counts(self):
        frames = [(csu.parse_su_line(l)[0], csu.parse_su_line(l)[1], None)
            for l in GCC_SU.splitlines() if csu.parse_su_line(l)]
        results = csu.check([("UserVariantBase::eval", 700, 1)], frames)
        self.assertEqual(600, results[0][2])
        self.assertIn("[clone .constprop.0]", results[0][3])


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
            "@build-type-flags -O3 -DNDEBUG\n@cxx-flags -fno-strict-aliasing -std=c++20\n")
        # the same flags in another order are the calibrated build, which is checked
        with build_dir(GCC_SU, build_type_flags="-DNDEBUG -O3", cxx_flags="-std=c++20 -fno-strict-aliasing") as d:
            b = write_budgets(d, "1 UserVariantBase::eval\n", settings)
            rc, out, _ = run(["--budgets", b, str(d)])
            self.assertEqual(1, rc, out)
        # other flags are not: link-time optimization, hardening flags, another optimization level
        for kwargs, text in (({"cxx_flags": "-fno-strict-aliasing -std=c++20 -flto"}, "cxx-flags"),
                ({"cxx_flags": "-O2 -fstack-protector-strong -fno-strict-aliasing -std=c++20"}, "cxx-flags"),
                ({"build_type_flags": "-O2 -DNDEBUG"}, "build-type-flags")):
            with build_dir(GCC_SU, **kwargs) as d:
                b = write_budgets(d, "1 UserVariantBase::eval\n", settings)
                rc, out, _ = run(["--budgets", b, str(d)])
                self.assertEqual(0, rc, out)
                self.assertIn("SKIPPED", out)
                self.assertIn(text, out)

    def test_part_clone_adds(self):
        # a head and its .part clone are both on the stack: their frames add; other clones are alternatives
        su = ("/src/lib/A.cpp:1:1:int A::f(int)\t100\tstatic\n"
            "/src/lib/A.cpp:1:1:int A::f(int) [clone .constprop.0]\t120\tstatic\n"
            "/src/lib/A.cpp:1:1:int A::f(int) [clone .part.0]\t300\tstatic\n"
            "/src/lib/A.cpp:1:1:int A::f(int) [clone .part.1]\t200\tstatic\n"
            "/src/lib/A.cpp:2:1:int A::g(int) [clone .part.0]\t80\tstatic\n")
        frames = [(csu.parse_su_line(l)[0], csu.parse_su_line(l)[1], None) for l in su.splitlines()]
        results = csu.check([("A::f", 500, 1), ("A::g", 500, 2)], frames)
        self.assertEqual(420, results[0][2])
        self.assertIn("[clone .constprop.0] + ", results[0][3])
        # a function inlined into its callers whose .part clone remains has the frame of the clone
        self.assertEqual(80, results[1][2])

    def test_input_errors(self):
        with build_dir(GCC_SU, write_config=False) as d:
            b = write_budgets(d, "600 UserVariantBase::eval\n")
            rc, _, err = run(["--budgets", b, str(d)])
            self.assertEqual(2, rc)
            self.assertIn("QORE_STACK_USAGE_REPORT", err)
        with build_dir(None) as d:
            b = write_budgets(d, "600 UserVariantBase::eval\n")
            rc, _, err = run(["--budgets", b, str(d)])
            self.assertEqual(2, rc)
            self.assertIn("no .su files", err)
            rc, _, err = run(["--budgets", str(Path(d) / "none.txt"), str(d)])
            self.assertEqual(2, rc)
            self.assertIn("cannot read", err)


if __name__ == "__main__":
    unittest.main()
