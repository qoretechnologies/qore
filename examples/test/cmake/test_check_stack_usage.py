#!/usr/bin/env python3
"""Tests of the stack usage check (tools/check-stack-usage.py, tools/stack-usage-budgets.txt; issue #5852).

- the budget file of the repository parses, names each function once and is calibrated for a compiler and build
  type;
- .su records of GCC and Clang are parsed, and a budget matches a function's own frame only: not a lambda or local
  class inside it, not a function of another class whose name ends with the same name, and every clone of it;
- the frame of a function is its largest record; a function with a .part clone (partial inlining) in the symbol
  table fails as SPLIT, not its .cold fragments (with g++ and objdump: the samples of GCC's clones in
  stack-usage-samples);
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
import platform
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
            frames.append((rec[0], rec[1], path, rec[2], csu.su_line_dynamic(line)))
    return frames


def run(argv, allow_no_symbols=True):
    """Runs the check; the fixtures have .su files without object files, so their symbol tables are not required
    unless allow_no_symbols is False"""
    out = io.StringIO()
    err = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        rc = csu.main((["--allow-no-symbols"] if allow_no_symbols else []) + argv)
    return rc, out.getvalue(), err.getvalue()


def write_budgets(tmp, body,
        settings="@compiler GNU\n@build-type Release\n@compiler-version 15.2.0\n@target x86_64-linux-gnu\n"):
    path = Path(tmp) / "budgets.txt"
    path.write_text(settings + body)
    return str(path)


# the frames that check-stack-usage.py --force reported for the release builds in CI's arm64 images (GCC 15.2):
# Ubuntu (aarch64-linux-gnu) and Alpine (aarch64-alpine-linux-musl)
ARM64_RELEASE_FRAMES = {
    "aarch64-linux-gnu": {
        "QoreIRInterpreter::execute": 3488, "tryExecuteInterpreterInlineIRFunction": 768,
        "tryExecuteInterpreterInlineIRStaticMethod": 784, "evalAndRef": 48, "AbstractQoreNode::eval": 64,
        "AbstractMethodCallNode::exec": 160, "NormalMethodFunction::evalMethodTmpArgs": 768,
        "StaticMethodFunction::evalMethodTmpArgs": 768, "qore_method_private::evalNormalVariant": 816,
        "UserMethodVariant::evalMethod": 64, "UserVariantBase::eval": 416, "UserClosureVariant::evalClosure": 464,
        "UserVariantBase::evalTiered": 272, "UserVariantBase::evalIntern": 128, "QoreFunction::evalFunction": 992,
        "QoreFunction::evalFunctionTmpArgs": 1008, "UserFunctionVariant::evalFunction": 0,
        "FunctionCallNode::evalImpl": 128, "StaticMethodCallNode::evalImpl": 160, "StatementBlock::execIntern": 288,
        "StatementBlock::execImpl": 224, "AbstractStatement::exec": 304, "ReturnStatement::execImpl": 112,
    },
}
ARM64_RELEASE_FRAMES["aarch64-alpine-linux-musl"] = dict(ARM64_RELEASE_FRAMES["aarch64-linux-gnu"],
    **{"QoreIRInterpreter::execute": 3504})
ARM64_HARDENING = {
    "aarch64-linux-gnu": "_FORTIFY_SOURCE=3 __SSP_STRONG__=3",
    "aarch64-alpine-linux-musl": "_FORTIFY_SOURCE=2 __SSP_STRONG__=3",
}


class RepositoryBudgetsTest(unittest.TestCase):
    def test_budget_files(self):
        # every budget file parses, names the same functions, is for its own target, and is chosen for it
        files = sorted((ROOT / "tools").glob("stack-usage-budgets*.txt"))
        targets = {}
        names = None
        for path in files:
            settings, budgets = csu.parse_budgets(path.read_text(), str(path))
            self.assertNotIn(settings["target"], targets, path)
            targets[settings["target"]] = path
            if names is None:
                names = [b[0] for b in budgets]
            self.assertEqual(names, [b[0] for b in budgets], path)
            self.assertEqual(path, csu.select_budgets(settings["target"]))
        self.assertEqual(sorted(["x86_64-linux-gnu", "aarch64-linux-gnu", "aarch64-alpine-linux-musl"]),
            sorted(targets))
        self.assertEqual(csu.DEFAULT_BUDGETS, csu.select_budgets("powerpc64le-linux-gnu"))

    def test_arm64_calibration(self):
        # the release frames measured in CI's arm64 images pass their target's budgets (chosen by the target of the
        # build), by the margin of the budgets, and the x86_64 budgets do not fit them (StatementBlock::execImpl)
        for target, frames in ARM64_RELEASE_FRAMES.items():
            su = "".join("/src/lib/x.cpp:%d:1:int %s(int)\t%d\tstatic\n" % (i, f, size)
                for i, (f, size) in enumerate(sorted(frames.items()), 1))
            with build_dir(su, target=target, hardening=ARM64_HARDENING[target]) as d:
                rc, out, _ = run([str(d)])
                self.assertEqual(0, rc, out)
                self.assertIn("for %s; budgets calibrated for GNU, Release build for %s" % (target, target), out)
                self.assertIn("OK: 23 function(s) within budget", out)
                rc, out, _ = run(["--budgets", str(csu.DEFAULT_BUDGETS), "--force", str(d)])
                self.assertEqual(1, rc, out)
                self.assertRegex(out, r"OVER\s+StatementBlock::execImpl")
            settings, budgets = csu.parse_budgets(csu.select_budgets(target).read_text())
            for func, budget, _ in budgets:
                # a margin of at least 32 bytes, but one array of 528 bytes more in a frame fails
                self.assertGreaterEqual(budget - frames[func], 32, func)
                self.assertLess(budget, frames[func] + 528, func)

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

    def test_symbol_names(self):
        self.assertEqual("{anonymous}::K::f",
            csu.symbol_function(csu.Symbol("(anonymous namespace)::K::f(int) [clone .part.0]", ".text", 0, 1).name))
        self.assertEqual("N::K::tbig", csu.symbol_function("int N::K::tbig<char>(char) [clone .part.0]"))
        self.assertEqual("K::big", csu.symbol_function("K::big[abi:cxx11](int) const [clone .part.0]"))
        self.assertEqual("K::f", csu.symbol_function("K::f(int (*)(int), std::map<int, int> const&)"))
        self.assertIsNone(csu.symbol_function("not a function"))

    def test_template_names(self):
        # the template arguments of every scope and ABI tags are ignored, "operator<" is a name
        self.assertEqual("S::h", csu.strip_templates("S<int>::h"))
        self.assertEqual("N::K::f", csu.strip_templates("N::K<std::map<int, int> >::f[abi:cxx11]<char>"))
        self.assertEqual("A::operator<", csu.strip_templates("A::operator<"))
        self.assertEqual("A::operator<<", csu.strip_templates("A::operator<<"))
        self.assertTrue(csu.matches("S::h", "static int S<T>::h(int) [with T = int]"))
        self.assertTrue(csu.matches("T::f", "int T<N>::f(int) [with int N = 1]"))
        self.assertFalse(csu.matches("K::f", "int K::f(int)::<lambda()>"))

    def test_split(self):
        # a .part clone of a budgeted function (any overload or template instance) in the symbol table fails the
        # check as SPLIT; a .cold fragment does not count, nor the .part clone of another function
        su = "/src/d.cpp:15:5:int K::f(int)\t8\tstatic\n"
        cases = (
            ([csu.Symbol("K::f(int) [clone .part.0]"), csu.Symbol("K::f(int)")], "K::f(int) [clone .part.0]"),
            ([csu.Symbol("K::f(long) [clone .part.0]")], "K::f(long) [clone .part.0]"),
            ([csu.Symbol("int K<int>::f<char>(char) [clone .part.0]")], "int K<int>::f<char>(char) [clone .part.0]"),
            ([csu.Symbol("K::f[abi:cxx11](int) [clone .part.0] [clone .constprop.0]")],
                "K::f[abi:cxx11](int) [clone .part.0] [clone .constprop.0]"),
            ([csu.Symbol("K::f(int) [clone .part.0] [clone .cold]"), csu.Symbol("K::f(int) [clone .cold]")], None),
            ([csu.Symbol("K::g(int) [clone .part.0]"), csu.Symbol("XK::f(int) [clone .part.0]")], None),
            ([csu.Symbol("K::f(int) [clone .constprop.0] [clone .isra.0]")], None),
        )
        for symbols, split in cases:
            with build_dir(su) as d:
                b = write_budgets(d, "100 K::f\n")
                with unittest.mock.patch.object(csu, "object_symbols", return_value=symbols):
                    rc, out, _ = run(["--budgets", b, str(d)], allow_no_symbols=False)
            if split:
                self.assertEqual(1, rc, out)
                self.assertRegex(out, r"SPLIT\s+K::f\s+budget\s+100 B: K::f was split by partial inlining")
                self.assertIn(split, out)
            else:
                self.assertEqual(0, rc, out)
                self.assertIn("OK: 1 function(s) within budget", out)

    def test_largest_record(self):
        # the frame is the largest record of the function, all overloads (.constprop and .isra clones are recorded
        # under its name); "0(...)" records are not its
        su = ("/src/A.cpp:16:5:int N::K::big(int)\t576\tstatic\n"
            "/src/A.cpp:16:5:int N::K::big(int)\t8\tstatic\n"
            "/src/A.cpp:16:5:0(int)\t900\tstatic\n"
            "/src/A.cpp:20:5:int N::K::big(int, int)\t600\tstatic\n")
        results = csu.check([("N::K::big", 1, 1)], frames_of(su))
        self.assertEqual(600, results[0][2])
        self.assertIsNone(results[0][4])
        self.assertTrue(csu.is_part_clone("0(int)"))
        self.assertFalse(csu.is_part_clone("int big(int)"))


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

    def test_missing_symbols(self):
        # without the object file of a .su file with records of a budgeted function, the clones that are not at the
        # function's location cannot be found: an error, or a note with --allow-no-symbols
        with build_dir(GCC_SU) as d:
            b = write_budgets(d, "600 UserVariantBase::eval\n")
            rc, out, err = run(["--budgets", b, str(d)], allow_no_symbols=False)
            self.assertEqual(2, rc, out)
            self.assertIn("symbol tables of the object files", err)
            rc, out, _ = run(["--budgets", b, str(d)])
            self.assertEqual(0, rc, out)
            self.assertIn("note: the symbol tables", out)
        # a .su file without records of budgeted functions needs none
        with build_dir("/src/lib/other.cpp:1:1:int other(int)\t8\tstatic\n") as d:
            b = write_budgets(d, "8 another\n")
            rc, out, _ = run(["--budgets", b, str(d)], allow_no_symbols=False)
            self.assertEqual(1, rc, out)
            self.assertIn("MISSING", out)

    @unittest.skipUnless(shutil.which("g++") and shutil.which("objdump"), "g++ or objdump is not available")
    def test_samples_with_gcc(self):
        # GCC's clones in the samples (examples/test/cmake/stack-usage-samples), with g++ -O3: a function that
        # partial inlining split is SPLIT (also when identical code folding merged its .part clone with another
        # function's, when its head is inlined into all callers, for template instances and members of class
        # templates, with an ABI tag, and next to .cold fragments and .constprop variants); an unsplit function's
        # frame is its largest record, also with .constprop and .isra clones.  The frames are those of GCC 15, so
        # other versions are skipped
        version = subprocess.run(["g++", "-dumpversion"], stdout=subprocess.PIPE, universal_newlines=True).stdout
        if version.strip().split(".")[0] != "15":
            self.skipTest("the frames are those of GCC 15, and g++ is %s" % version.strip())
        split = "split"
        expected = {
            "abi-tag.cpp": {"K::big": split, "c3": 16},
            "anonymous-namespace.cpp": {"{anonymous}::K::f": split},
            "cold-fragment.cpp": {"K::f": split, "g": split, "c": 48},
            "folded-into-class-template.cpp": {"S::h": split, "g": split, "z": split, "c3": 32},
            "folded-into-inlined-static-template.cpp": {"tf": split, "z": split, "c2": 32},
            "folded-into-inlined-static.cpp": {"g": split, "z": split, "call": 32},
            "folded-larger-own.cpp": {"K::f": split, "K::z": split, "c": 64},
            "folded-overload.cpp": {"K::f": split, "K::z": split, "Q::g": split, "c": 48},
            "folded-same-arity.cpp": {"K::f": split},
            "inlined-head-zero-record.cpp": {"{anonymous}::L::f": split, "K::f": 96, "call2": 48},
            "member-const-char.cpp": {"K::b": split},
            # an overload split next to one with .constprop/.isra clones; template instances
            "overloads-templates.cpp": {"N::K::big": split, "tf": split, "tf2": 304, "M::K::big": 352, "v2": 16},
            # a .part clone with a .constprop variant (member and free function); a static function with a
            # .constprop clone, not split
            "part-constprop.cpp": {"K::f": split, "g": 1264},
            "part-constprop-free.cpp": {"K::f": split, "g": split},
        }
        samples = ROOT / "examples" / "test" / "cmake" / "stack-usage-samples"
        self.assertEqual(sorted(expected), sorted(p.name for p in samples.glob("*.cpp")))
        for name, funcs in expected.items():
            with tempfile.TemporaryDirectory() as tmp:
                tmp = Path(tmp)
                shutil.copy(str(samples / name), str(tmp / name))
                subprocess.run(["g++", "-O3", "-fstack-usage", "-c", name, "-o", name + ".o"], cwd=str(tmp),
                    check=True)
                results = {r[0]: r for r in csu.check([(f, 1, 1) for f in funcs], csu.collect_frames(tmp))}
            for func, value in funcs.items():
                label = "%s: %s: %r" % (name, func, results[func])
                self.assertIsNone(results[func][5], label)
                if value == split:
                    self.assertIsNotNone(results[func][4], label)
                    self.assertIn("[clone .part.", results[func][4], label)
                else:
                    self.assertIsNone(results[func][4], label)
                    self.assertEqual(value, results[func][2], label)


# a body that GCC compiles to the same code wherever it is used, so that identical code folding merges its copies
ICF_BODY = """
    char buf[1000]; memset(buf, x, sizeof buf); use(buf); use(buf + 1); use(buf + 9);
    for (int i = 0; i < x; ++i) { char b2[300]; memset(b2, i, sizeof b2); use(b2); use(b2 + 1); }
    use(buf + 4); use(buf + 5); return buf[9];
"""

# a member of a class merged into an identical one (the wrapper is "endbr64; jmp A::a(int)"), static functions whose
# addresses are taken (also merged, as wrappers, not aliases), and one with different code (not merged)
FOLDED_CPP = """
#include <cstring>
extern void use(char*);
struct A { static int a(int x); };
struct B { static int b(int x); };
int A::a(int x) {%(body)s}
int B::b(int x) {%(body)s}
static int sa(int x) {%(body)s}
static int sb(int x) {%(body)s}
int (*pa)(int) = sa;
int (*pb)(int) = sb;
int other(int x) { char buf[100]; memset(buf, x, sizeof buf); use(buf); return buf[x]; }
""" % {"body": ICF_BODY}

# variable-length arrays and alloca(): "dynamic" records; a VLA with a known bound: "dynamic,bounded"
DYNAMIC_CPP = """
#include <cstring>
extern void use(char*);
int vla(int x) { char big[x * 100]; memset(big, x, x * 100); use(big); return big[3]; }
int dyn_alloca(int x) { char* p = (char*)__builtin_alloca(x); memset(p, x, x); use(p); return p[3]; }
int fixed(int x) { char buf[100]; memset(buf, x, sizeof buf); use(buf); return buf[3]; }
"""

# overloads: K::f(int) calls K::f(double) and uses the result (both frames on the stack); K::g(int) only branches to
# K::g(double) (a tail call, which reuses the frame)
OVERLOAD_CPP = """
#include <cstring>
extern void use(char*);
struct K {
    static int f(int x);
    static int f(double x);
    static int g(int x);
    static int g(double x);
};
__attribute__((noinline)) int K::f(double x) { char buf[500]; memset(buf, (int)x, sizeof buf); use(buf);
    return buf[3]; }
int K::f(int x) { char buf[600]; memset(buf, x, sizeof buf); use(buf); int r = f((double)x); use(buf + r);
    return buf[r] + r; }
__attribute__((noinline)) int K::g(double x) { char buf[500]; memset(buf, (int)x + 1, sizeof buf); use(buf);
    return buf[4]; }
int K::g(int x) { return g((double)(x + 1)); }
"""


@unittest.skipUnless(shutil.which("g++") and shutil.which("objdump"), "needs g++ and objdump")
class ObjectCodeTest(unittest.TestCase):
    """Functions compiled with g++ -O3 whose records do not show their stack use"""

    def compile(self, tmp, source):
        (tmp / "t.cpp").write_text(source)
        subprocess.run(["g++", "-O3", "-fstack-usage", "-c", "t.cpp", "-o", "t.cpp.o"], cwd=str(tmp), check=True)

    def check(self, source, funcs):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            self.compile(tmp, source)
            return {r[0]: r for r in csu.check([(f, 100000, 1) for f in funcs], csu.collect_frames(tmp))}

    @unittest.skipUnless(platform.machine() in ("x86_64", "AMD64"), "the wrapper of identical code folding is that of "
        "x86_64 (endbr64; jmp)")
    def test_folded(self):
        results = self.check(FOLDED_CPP, ["A::a", "B::b", "sa", "sb", "other"])
        # the record of a merged function is that of its wrapper (a few bytes), not of the code it runs
        self.assertLess(results["B::b"][2], 64)
        self.assertEqual(("FOLDED", "B::b was merged by identical code folding into A::a(int); budget A::a(int) or "
            "make the bodies differ"), results["B::b"][5])
        # static functions whose addresses are taken: wrappers of the same function
        for func in ("sa", "sb"):
            self.assertEqual("FOLDED", results[func][5][0], results[func])
            self.assertIn("%s was merged by identical code folding into A::a(int)" % func, results[func][5][1])
        for func in ("A::a", "other"):
            self.assertIsNone(results[func][5], results[func])
        # the status is printed, and the check fails
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            self.compile(tmp, FOLDED_CPP)
            (tmp / "stack-usage-config.txt").write_text("compiler=GNU\nbuild-type=Release\ntarget=x86_64-linux-gnu\n")
            budgets = tmp / "b.txt"
            budgets.write_text("@compiler GNU\n@build-type Release\n@target x86_64-linux-gnu\n100000 A::a\n64 B::b\n")
            code, out, err = run(["--budgets", str(budgets), str(tmp)], allow_no_symbols=False)
            self.assertEqual(1, code, out + err)
            self.assertRegex(out, r"FOLDED +B::b +budget +64 B: B::b was merged by identical code folding into "
                r"A::a\(int\); budget A::a\(int\) or make the bodies differ")
            self.assertRegex(out, r"OK +A::a ")

    def test_dynamic(self):
        results = self.check(DYNAMIC_CPP, ["vla", "dyn_alloca", "fixed"])
        for func in ("vla", "dyn_alloca"):
            self.assertEqual("DYNAMIC", results[func][5][0], results[func])
            self.assertIn("known only at run time", results[func][5][1])
        self.assertIsNone(results["fixed"][5], results["fixed"])

    def test_dynamic_records(self):
        # "dynamic,bounded" includes the allocation; "dynamic" does not; a function with any "dynamic" record fails
        su = ("/src/lib/a.cpp:1:1:int f(int)\t32\tdynamic\n/src/lib/a.cpp:2:1:int g(int)\t96\tdynamic,bounded\n"
            "/src/lib/a.cpp:3:1:int h(int)\t96\tstatic\n/src/lib/a.cpp:4:1:int h(double)\t16\tdynamic\n")
        self.assertTrue(csu.su_line_dynamic(su.splitlines()[0]))
        self.assertFalse(csu.su_line_dynamic(su.splitlines()[1]))
        self.assertFalse(csu.su_line_dynamic(su.splitlines()[2]))
        self.assertFalse(csu.su_line_dynamic("a.cpp:1:1:int f(int)\t32"))
        results = {r[0]: r for r in csu.check([("f", 100, 1), ("g", 100, 1), ("h", 100, 1)], frames_of(su))}
        self.assertEqual("DYNAMIC", results["f"][5][0])
        self.assertIsNone(results["g"][5])
        self.assertEqual(96, results["g"][2])
        self.assertEqual("DYNAMIC", results["h"][5][0])
        self.assertIn("int h(double)", results["h"][5][1])
        with build_dir(su) as tmp:
            budgets = write_budgets(tmp, "100 f\n100 g\n100 h\n")
            code, out, err = run(["--budgets", str(budgets), str(tmp)])
            self.assertEqual(1, code, out + err)
            self.assertRegex(out, r"DYNAMIC +f ")
            self.assertRegex(out, r"OK +g ")
            self.assertRegex(out, r"DYNAMIC +h ")

    @unittest.skipUnless(platform.machine() in ("x86_64", "AMD64", "aarch64", "arm64"), "x86_64 and aarch64 calls")
    def test_overload_call(self):
        results = self.check(OVERLOAD_CPP, ["K::f", "K::g"])
        self.assertEqual(("OVERLOAD-CALL", "K::f(int) calls K::f(double): both frames are on the stack, while the "
            "budget is checked against the larger; budget the sum or avoid the call"), results["K::f"][5])
        # a tail call
        self.assertIsNone(results["K::g"][5], results["K::g"])


if __name__ == "__main__":
    unittest.main()
