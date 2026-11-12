#!/usr/bin/env python3
"""Tests of the stack usage check (tools/check-stack-usage.py, tools/stack-usage-budgets.txt; issue #5852).

- the budget file of the repository parses, names each function once and is calibrated for a compiler and build
  type;
- .su records of GCC and Clang are parsed, and a budget matches a function's own frame only: not a lambda or local
  class inside it, not a function of another class whose name ends with the same name, and every clone of it;
- a function split by partial inlining counts its frame and that of its .part clone, which GCC records as
  "0(<params>)" at the function's location (a free or static member function) or under the function's own name there
  (a non-static member function or a template instance, told from .constprop and .isra clones with nm), also with the
  real g++ and nm when they are available; .constprop and .isra clones count only the largest;
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
import unittest.mock

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
    # an overload of the function: the largest of them counts
    "/src/lib/Function.cpp:60:10:QoreValue UserVariantBase::eval(const char*, ExceptionSink*) const\t600\t"
    "dynamic,bounded\n"
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
        # template instances, as GCC's .su records and nm name them
        self.assertTrue(csu.matches("tf", "int tf(T) [with T = int]"))
        self.assertTrue(csu.matches("N::K::tbig", "int N::K::tbig<char>(char) [clone .part.0]"))
        self.assertFalse(csu.matches("N::K::tbig", "int N::K::tbigger<char>(char)"))
        self.assertTrue(csu.matches("N::K::big", "N::K::big(int) [clone .part.0]"))
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

    def test_part_clone_names(self):
        # the .part clone that nm names belongs to the function with the same name and parameters
        sig = "int N::K::big(int)"
        self.assertTrue(csu.is_part_clone_of("N::K::big", sig, "N::K::big(int) [clone .part.0]"))
        self.assertTrue(csu.is_part_clone_of("N::K::big", sig, "N::K::big[abi:cxx11](int) [clone .part.0]"))
        self.assertFalse(csu.is_part_clone_of("N::K::big", "int N::K::big(int, int)", "N::K::big(int) [clone .part.0]"))
        self.assertFalse(csu.is_part_clone_of("N::K::big", sig, "N::K::bigger(int) [clone .part.0]"))
        # a template instance is matched by its name only (.su records give the template parameters)
        self.assertTrue(csu.is_part_clone_of("tf", "int tf(T) [with T = int]", "int tf<char>(char) [clone .part.0]"))
        self.assertEqual("int, int", csu.match_params("N::K::big", "int N::K::big(int,  int) const"))

    def test_part_clone_from_nm_fixture(self):
        # the records of an overload whose .part clone nm lists are added; those of an overload with .constprop and
        # .isra clones are not (nm output as a fixture, so that this runs without g++)
        su = ("/src/d.cpp:15:5:int N::K::big(int)\t576\tstatic\n"
            "/src/d.cpp:16:31:int N::K::big(int, int)\t1632\tstatic\n"
            "/src/d.cpp:15:5:int N::K::big(int)\t8\tstatic\n"
            "/src/d.cpp:16:31:int N::K::big(int, int)\t1632\tstatic\n")
        nm = ["N::K::big(int) [clone .part.0]", "N::K::big(int, int) [clone .constprop.0] [clone .isra.0]"]
        nm = [n for n in nm if "[clone .part." in n]
        frames = frames_of(su, Path("/src/d.cpp.su"))
        with unittest.mock.patch.object(csu, "object_part_clones", return_value=nm):
            results = csu.check([("N::K::big", 1, 1)], frames)
        self.assertEqual(1632, results[0][2])
        with unittest.mock.patch.object(csu, "object_part_clones", return_value=nm):
            results = csu.check([("N::K::big", 1, 1)], [f for f in frames if f[1] != 1632])
        self.assertEqual(584, results[0][2])

    def test_same_location_records_without_object(self):
        # two records of the same signature at the same location are a .part clone and its head, or a .constprop or
        # .isra clone and the function; without the object file to tell them apart, they are added
        su = ("/src/A.cpp:16:5:int N::K::big(int)\t576\tstatic\n"
            "/src/A.cpp:16:5:int N::K::big(int)\t8\tstatic\n")
        results = csu.check([("N::K::big", 1000, 1)], frames_of(su, Path("/nonexistent/A.cpp.su")))
        self.assertEqual(584, results[0][2])
        self.assertIn("assumed", results[0][3])


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

    @unittest.skipUnless(shutil.which("g++") and shutil.which("nm"), "g++ or nm is not available")
    def test_part_clones_with_gcc(self):
        # functions that GCC splits by partial inlining at -O3: the cold part with the large arrays becomes a .part
        # clone that the callers call after the inlined head; GCC records the clone of a free function as "0(int)",
        # and that of a non-static member function or a template instance under the function's own name
        # the bodies differ, so that identical code folding does not merge the .part clones
        source = """#include <cstring>
extern void use(char*);
extern int flag;
extern int g(int*);
#define BODY(test, k) if (test) return 1; \\
    char buf[300]; memset(buf, x, sizeof buf); use(buf); use(buf+1); use(buf+2); use(buf+k); \\
    for (int i = 0; i < x; ++i) { char b2[200]; memset(b2, i, sizeof b2); use(b2); use(b2+1); use(b2+2);} \\
    use(buf+4); use(buf+5); use(buf+6); use(buf+7); use(buf+8); return buf[k];
int big(int x) { BODY(flag, 3) }
namespace N {
struct K {
    int m;
    int big(int x);
    // an overload with .constprop and .isra clones, at another location
    int big(int a, int b);
};
int K::big(int x) { BODY(m, 9) }
int __attribute__((noinline)) K::big(int a, int b) {
    int x[400]; for (int i = 0; i < b; ++i) x[i & 63] = a + i + m; return g(x);
}
}
// a member function with an ABI tag, which nm writes after the name: "A::big[abi:cxx11](int) [clone .part.0]"
struct A {
    int m;
    [[gnu::abi_tag("cxx11")]] int big(int x);
};
int A::big(int x) { BODY(m, 12) }
template <typename T> int tf(T x) { BODY(flag, sizeof(T) + 10) }
// a function with .constprop and .isra clones, which are not frames on top of it
struct S {
    int m;
    int __attribute__((noinline)) mm(int a, int b) {
        int x[64]; for (int i = 0; i < b; ++i) x[i & 63] = a + i + m; return g(x);
    }
};
int c1(int n) { return big(n) + 1; }
int c2(int n) { return big(n * 2) + 2; }
int c3(N::K* k, int n) { return k->big(n) + 1; }
int c4(N::K* k, int n) { return k->big(n * 2) + 2; }
int c5(int n) { return tf<int>(n) + tf<int>(n * 3); }
int c6(A* a, int n) { return a->big(n) + 1; }
int c7(A* a, int n) { return a->big(n * 2) + 2; }
int w1(N::K* k, int a) { return k->big(a, 5); }
int w2(N::K* k, int a) { return k->big(a, 5) + 1; }
int w3(N::K* k, int a, int b) { return k->big(a, b); }
int v1(S* s, int a) { return s->mm(a, 5); }
int v2(S* s, int a) { return s->mm(a, 5) + 1; }
int v3(S* s, int a, int b) { return s->mm(a, b); }
"""
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            (tmp / "p.cpp").write_text(source)
            subprocess.run(["g++", "-O3", "-fstack-usage", "-c", "p.cpp", "-o", "p.cpp.o"], cwd=str(tmp), check=True)
            frames = csu.collect_frames(tmp)
            parts = csu.object_part_clones(tmp / "p.cpp.su", {})
            self.assertIsNotNone(parts)

            def records(func):
                return sorted((f[1] for f in frames if csu.matches(func, f[0])), reverse=True)

            results = {r[0]: r for r in csu.check([("big", 1, 1), ("N::K::big", 1, 2), ("tf", 1, 3), ("S::mm", 1, 4),
                ("A::big", 1, 5)], frames)}

            # the free function: its head and the "0(int)" record at its location
            zero = [f for f in frames if csu.is_part_clone(f[0])]
            big = [f for f in frames if csu.matches("big", f[0])]
            if not zero or not any(csu.matches("N::K::big", n) for n in parts):
                self.skipTest("this g++ does not split the functions by partial inlining")
            self.assertEqual(1, len(big))
            self.assertIn(big[0][3], [f[3] for f in zero])
            self.assertEqual(big[0][1] + max(f[1] for f in zero if f[3] == big[0][3]), results["big"][2])

            # the non-static member function: two records of its own name, head and .part clone, added; its overload
            # has .constprop and .isra clones at another location, which are not added, although the other overload
            # has a .part clone
            member = sorted((f[1] for f in frames if f[0] == "int N::K::big(int)"), reverse=True)
            overload = sorted((f[1] for f in frames if f[0] == "int N::K::big(int, int)"), reverse=True)
            self.assertEqual(2, len(member))
            self.assertGreater(member[0], 300)
            self.assertGreater(len(overload), 1)
            self.assertEqual(max(member[0] + member[1], overload[0]), results["N::K::big"][2])
            self.assertEqual(member[0] + member[1],
                csu.check([("N::K::big", 1, 1)], [f for f in frames if f[0] != "int N::K::big(int, int)"])[0][2])
            self.assertEqual(overload[0],
                csu.check([("N::K::big", 1, 1)], [f for f in frames if f[0] != "int N::K::big(int)"])[0][2])

            # the member function with an ABI tag: its .part clone is found by nm under the tagged name
            self.assertTrue(any("[abi:cxx11]" in n for n in parts))
            tagged = records("A::big")
            self.assertEqual(2, len(tagged))
            self.assertEqual(tagged[0] + tagged[1], results["A::big"][2])

            # the template instance: its head is inlined into the caller, only the .part clone has a frame
            self.assertTrue(any(csu.matches("tf", n) for n in parts))
            self.assertEqual(records("tf")[0], results["tf"][2])

            # .constprop and .isra clones under the same name and location are alternatives: the largest counts
            self.assertFalse(any(csu.matches("S::mm", n) for n in parts))
            self.assertGreater(len(records("S::mm")), 1)
            self.assertEqual(records("S::mm")[0], results["S::mm"][2])


if __name__ == "__main__":
    unittest.main()
