#!/usr/bin/env python3
"""The libqore that the programs of a test run with a build load (issue #7029)

On macOS the dynamic loader ignores LD_LIBRARY_PATH, which run_tests.sh sets: a program finds libqore through the
directories recorded in it (LC_RPATH), and a program that qcc compiles records the directory of the libqore that qcc
loaded.  DYLD_LIBRARY_PATH would not help, as macOS removes it from the environment of a protected program such as
/bin/sh, whose children then do not get it.  So run_tests.sh refuses a run with a build whose qore, or the qcc that the
tests use, loads a libqore other than the build's (the loader reports it with DYLD_PRINT_LIBRARIES), or cannot be
checked (not a Mach-O program, or no libqore reported): a build directory copied from another tree, QCC naming the qcc
of another build or installation, or the qcc in the PATH that the tests take when the build has none.  On every
platform it refuses a QORE_LIBDIR that is not the directory of the build's libqore: qcc links the programs that it
compiles with the libqore there and records that directory in them.

It also leaves QORE_BIN out of the environment of the tests: TestInterpreter.qm takes the interpreter from it before
QORE_BINARY, so one left in a developer's environment would run another qore, with its own libqore, in their child
processes.

The macOS cases run the qore of the build in QORE_TEST_BUILD_DIR, or in build/ in this tree, and are skipped without
one; they allow a build that does not match the tree (QORE_TEST_ALLOW_BUILD_MISMATCH, #6897), which their stand-in
builds are not, so that only the check of the libqore decides.  The cases of QORE_BIN and QORE_LIBDIR use a stand-in for
qore.  The runner tested is QORE_RUN_TESTS_SH if set (to show that a
case fails with an older run_tests.sh), else the one in this tree.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_watchdog import BUILD_DIR, HAVE_QORE, LIBQORE, RunnerTree  # noqa: E402

DARWIN = sys.platform == "darwin"
HAVE_QCC = HAVE_QORE and (BUILD_DIR / "qcc").exists()
HAVE_TOOLS = bool(shutil.which("install_name_tool") and shutil.which("codesign"))

#: a qore test that records that it ran in the file named by MARKER
QORE_MARKER_TEST = """\
    %modern
    File f();
    f.open2(ENV.MARKER, O_WRONLY | O_CREAT | O_TRUNC);
    f.write("ran\\n");
    """


def make_copy(source, target, rpath_to=None):
    """Copies a program or library; with rpath_to, the library directory recorded in the program is changed to it"""
    shutil.copy2(source, target)
    if rpath_to:
        commands = subprocess.run(["otool", "-l", str(target)], check=True, capture_output=True, text=True).stdout
        rpaths = re.findall(r"cmd LC_RPATH\n\s+cmdsize \d+\n\s+path (.*) \(offset \d+\)", commands)
        # a program linked against other shared libraries outside the build (for example qcc against an LLVM
        # installed in its own prefix) records their directories as well; only the one that holds libqore moves
        origin = str(Path(source).resolve().parent)
        libqore_rpaths = [path for path in rpaths if (Path(path.replace("@loader_path", origin)
                                                           .replace("@executable_path", origin))
                                                      / LIBQORE.name).exists()]
        if len(libqore_rpaths) != 1:
            raise RuntimeError(f"{source} records {rpaths}, not exactly one directory holding {LIBQORE.name}")
        subprocess.run(["install_name_tool", "-rpath", libqore_rpaths[0], str(rpath_to), str(target)], check=True,
                       capture_output=True)
        subprocess.run(["codesign", "-f", "-s", "-", str(target)], check=True, capture_output=True)


class QoreLibdirTest(unittest.TestCase):
    """QORE_LIBDIR must be the directory of the build's libqore"""

    def run_tree(self, libdir, missing=False):
        with tempfile.TemporaryDirectory(prefix="qore-runner-libqore-") as directory:
            tree = RunnerTree(directory)
            marker = tree.root / "marker"
            tree.add_test("marker.qtest", f"""\
                open({str(marker)!r}, "w").write("ran")
                """)
            other = tree.root / "installed/lib"
            other.mkdir(parents=True)
            variables = {"QORE_LIBDIR": str(libdir(tree, other))}
            if missing:
                variables = {"LIBQORE_BINARY": str(tree.root / "bin/libqore-missing.so")}
            result = tree.run(**variables)
            result.ran = marker.exists()
            return result

    def test_another_qore_libdir_is_refused(self):
        result = self.run_tree(lambda tree, other: other)
        self.assertNotEqual(0, result.returncode, result.output)
        self.assertIn("is not the directory of the libqore of the run", result.output)
        self.assertFalse(result.ran, result.output)

    def test_a_relative_qore_libdir_is_given_to_the_tests_as_an_absolute_path(self):
        """qcc records QORE_LIBDIR in the programs that it links as it is given: a relative one would be taken from
        the directory that a program runs in"""
        with tempfile.TemporaryDirectory(prefix="qore-runner-libqore-") as directory:
            tree = RunnerTree(directory)
            tree.add_test("libdir.qtest", """\
                import os
                import sys
                print("QORE_LIBDIR=%r" % os.environ.get("QORE_LIBDIR"))
                sys.exit(0 if os.path.isabs(os.environ.get("QORE_LIBDIR", "")) else 1)
                """)
            result = tree.run(QORE_LIBDIR="bin")
            self.assertEqual(0, result.returncode, result.output)
            self.assertIn("QORE_LIBDIR=%r" % str(Path(tree.root / "bin").resolve()), result.output)

    def test_a_missing_libqore_is_refused(self):
        """LIBQORE_BINARY naming no file would disable the check of the libqore that the programs load"""
        result = self.run_tree(lambda tree, other: tree.root / "bin", missing=True)
        self.assertNotEqual(0, result.returncode, result.output)
        self.assertIn("does not exist; set LIBQORE_BINARY to the build's libqore", result.output)
        self.assertFalse(result.ran, result.output)

    def test_the_build_libdir_runs(self):
        # the directory of the libqore of the run, by another path
        result = self.run_tree(lambda tree, other: tree.root / "installed/../bin")
        self.assertEqual(0, result.returncode, result.output)
        self.assertTrue(result.ran, result.output)


class QoreBinTest(unittest.TestCase):
    """The tests do not get QORE_BIN"""

    def test_qore_bin_is_not_passed_to_the_tests(self):
        with tempfile.TemporaryDirectory(prefix="qore-runner-libqore-") as directory:
            tree = RunnerTree(directory)
            tree.add_test("qore-bin.qtest", """\
                import os
                import sys
                print("QORE_BIN=%r" % os.environ.get("QORE_BIN"))
                sys.exit(1 if "QORE_BIN" in os.environ else 0)
                """)
            result = tree.run(QORE_BIN=str(tree.root / "other-build/qore"))
            self.assertEqual(0, result.returncode, result.output)
            self.assertIn("QORE_BIN=None", result.output)


@unittest.skipUnless(DARWIN, "the dynamic loader of macOS only")
@unittest.skipUnless(HAVE_QORE, f"no qore build in {BUILD_DIR} (QORE_TEST_BUILD_DIR)")
class LoadedLibqoreTest(unittest.TestCase):
    """A run whose qore or qcc loads a libqore other than the build's is refused before any test runs"""

    def run_tree(self, setup):
        """Runs a tree with the marker test; setup(tree) returns the variables of the run"""
        with tempfile.TemporaryDirectory(prefix="qore-runner-libqore-") as directory:
            tree = RunnerTree(directory, qore=True)
            tree.add_test("marker.qtest", QORE_MARKER_TEST)
            marker = tree.root / "marker"
            variables = setup(tree)
            result = tree.run(MARKER=str(marker), QORE_TEST_ALLOW_BUILD_MISMATCH="1", **variables)
            result.ran = marker.exists()
            return result

    @staticmethod
    def build_without_qcc(tree):
        """A build directory without qcc (the build's qore and libqore), and a decoy qcc first in the PATH"""
        build = tree.root / "build-without-qcc"
        build.mkdir()
        (build / "qore").symlink_to(BUILD_DIR / "qore")
        (build / LIBQORE.name).symlink_to(LIBQORE.resolve())
        (build / "CMakeCache.txt").write_text("# a build directory\n")
        decoy = tree.root / "decoy"
        decoy.mkdir()
        real = LIBQORE.resolve()
        make_copy(real, decoy / real.name)
        make_copy(BUILD_DIR / "qcc", decoy / "qcc", decoy)
        return {"QORE_BINARY": str(build / "qore"), "LIBQORE_BINARY": str(build / LIBQORE.name),
                "PATH": str(decoy) + ":" + tree.env()["PATH"]}

    def assert_refused(self, result, what):
        self.assertNotEqual(0, result.returncode, result.output)
        self.assertIn(f"ERROR: {what}", result.output)
        self.assertIn("the tests would run on another libqore (#7029)", result.output)
        self.assertFalse(result.ran, result.output)

    def test_the_build_runs(self):
        result = self.run_tree(lambda tree: {})
        self.assertEqual(0, result.returncode, result.output)
        self.assertTrue(result.ran, result.output)

    @unittest.skipUnless(HAVE_QCC, f"no qcc in {BUILD_DIR}")
    def test_the_build_runs_with_its_qcc_in_qcc(self):
        result = self.run_tree(lambda tree: {"QCC": str(BUILD_DIR / "qcc")})
        self.assertEqual(0, result.returncode, result.output)
        self.assertTrue(result.ran, result.output)

    def test_a_copied_build_directory_is_refused(self):
        """The qore of a copied build directory loads the libqore of the build that it was copied from"""
        def setup(tree):
            copy = tree.root / "copied-build"
            copy.mkdir()
            make_copy(BUILD_DIR / "qore", copy / "qore")
            real = LIBQORE.resolve()
            make_copy(real, copy / real.name)
            (copy / LIBQORE.name).symlink_to(real.name)
            (copy / "CMakeCache.txt").write_text("# a build directory\n")
            return {"QORE_BINARY": str(copy / "qore"), "LIBQORE_BINARY": str(copy / LIBQORE.name)}
        self.assert_refused(self.run_tree(setup), "the qore of the run")

    @unittest.skipUnless(HAVE_QCC and HAVE_TOOLS, f"no qcc in {BUILD_DIR}, or no install_name_tool and codesign")
    def test_a_qcc_that_loads_another_libqore_is_refused(self):
        """QCC names a qcc whose programs load its own libqore (a decoy: a copy of the build's that loads a copy of
        its libqore)"""
        def setup(tree):
            decoy = tree.root / "decoy"
            decoy.mkdir()
            real = LIBQORE.resolve()
            make_copy(real, decoy / real.name)
            make_copy(BUILD_DIR / "qcc", decoy / "qcc", decoy)
            return {"QCC": str(decoy / "qcc")}
        self.assert_refused(self.run_tree(setup), "the qcc that the tests use")

    @unittest.skipUnless(HAVE_QCC and HAVE_TOOLS, f"no qcc in {BUILD_DIR}, or no install_name_tool and codesign")
    def test_a_build_without_qcc_and_another_qcc_in_the_path_is_refused(self):
        """The tests of a build without qcc that look for qcc themselves take the one in the PATH (a decoy)"""
        setup = self.build_without_qcc
        result = self.run_tree(setup)
        self.assert_refused(result, "the qcc that the tests use")
        # how to fix it
        self.assertIn("Build the build's qcc (cmake --build <build> --target qcc)", result.output)

    @unittest.skipUnless(HAVE_QCC and HAVE_TOOLS, f"no qcc in {BUILD_DIR}, or no install_name_tool and codesign")
    def test_a_build_without_qcc_and_another_qcc_in_the_path_is_refused_with_qcc_set(self):
        """A test that reads only QORE_QCC_EXECUTABLE (not QCC) takes the qcc in the PATH (a decoy) when the build has
        none, whatever QCC names"""
        def setup(tree):
            variables = self.build_without_qcc(tree)
            variables["QCC"] = str(BUILD_DIR / "qcc")
            return variables
        self.assert_refused(self.run_tree(setup), "the qcc that the tests use")

    @unittest.skipUnless(HAVE_QCC, f"no qcc in {BUILD_DIR}")
    def test_a_qcc_script_is_refused(self):
        """A script cannot be asked which libqore it loads: one that runs another qcc would not be seen"""
        def setup(tree):
            script = tree.root / "qcc-wrapper"
            script.write_text(f"#!/bin/sh\nexec '{BUILD_DIR / 'qcc'}' \"$@\"\n")
            script.chmod(0o755)
            return {"QCC": str(script)}
        result = self.run_tree(setup)
        self.assertNotEqual(0, result.returncode, result.output)
        self.assertIn("is not a Mach-O program", result.output)
        self.assertFalse(result.ran, result.output)


if __name__ == "__main__":
    unittest.main()
