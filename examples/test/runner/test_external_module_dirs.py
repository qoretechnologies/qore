#!/usr/bin/env python3
"""The external module directories of a build in the module path of run_tests.sh (issue #5708)

cmake writes the directories of the installed modules that the qlib qmods of a build are compiled against to
qore-external-module-dirs.txt in the build directory, and run_tests.sh appends them to QORE_MODULE_DIR: a qmod requires
the modules it was compiled with, and a build whose prefix does not hold them otherwise loads the qmod's source instead
(BINARY-MODULE-SOURCE-FALLBACK).

run_tests.sh runs a tree of its own.  The cases of ModulePathTest use a stand-in for qore that runs each test file with
Python and records the module path it is given; those of BuildQoreTest run a qore test with the qore of the build in
QORE_TEST_BUILD_DIR, or in build/ in this tree, from a build directory of their own, and are skipped without one.  The
runner tested is QORE_RUN_TESTS_SH if set (to show that a case fails with an older run_tests.sh), else the one in this
tree.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import resource
import signal
import subprocess
import sys
import tempfile
import textwrap
import unittest


ROOT = Path(__file__).resolve().parents[3]
RUN_TESTS = Path(os.environ.get("QORE_RUN_TESTS_SH", str(ROOT / "run_tests.sh"))).resolve()

# the build whose qore runs the qore tests
BUILD_DIR = Path(os.environ.get("QORE_TEST_BUILD_DIR", str(ROOT / "build")))
LIBQORE = next((BUILD_DIR / name for name in ("libqore.so", "libqore.dylib") if (BUILD_DIR / name).exists()), None)
HAVE_QORE = (BUILD_DIR / "qore").exists() and LIBQORE is not None
HAVE_QCC = HAVE_QORE and (BUILD_DIR / "qcc").exists()

DIRS_FILE = "qore-external-module-dirs.txt"


def no_core_dumps():
    """Sets up a process of the harness: no core dumps, and the default action for the signals the runner traps"""
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    for sig in (signal.SIGHUP, signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, signal.SIG_DFL)


class RunnerTree:
    """A tree that run_tests.sh runs in, with a build directory (build/) that holds qore and libqore"""

    def __init__(self, directory, qore=False):
        self.qore = qore
        self.root = Path(directory)
        self.build = self.root / "build"
        self.build.mkdir()
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.tests = self.root / "examples/test"
        helpers = self.tests / "docker_test"
        helpers.mkdir(parents=True)
        # run_tests.sh reads its helpers from the tree it runs in
        (helpers / "ci-job-processes.sh").symlink_to(ROOT / "examples/test/docker_test/ci-job-processes.sh")
        self.tmp = self.root / "tmp"
        self.tmp.mkdir()
        self.cores = self.root / "cores"
        self.cores.mkdir()
        # the modules of the qore tests: QUnit and the others from the source of this tree
        (self.root / "qlib").symlink_to(ROOT / "qlib")
        if qore:
            # the qore of the build, from a build directory of this tree's own, which has no qmods and no binary
            # modules of its own
            (self.build / "qore").symlink_to(BUILD_DIR / "qore")
            (self.build / LIBQORE.name).symlink_to(LIBQORE)
        else:
            (self.build / "libqore.so").touch()
            # qore: runs the test file it is given with Python, in the same process
            self.write_script(self.build / "qore", f"""\
                #!/bin/sh
                for arg in "$@"; do
                    case "$arg" in
                        *.qtest) exec '{sys.executable}' -B "$arg" ;;
                    esac
                done
                echo "no test file in: $*" >&2
                exit 2
                """)
        # sudo: fails; with CI set, run_tests.sh sets the core file name of a macOS machine with "sudo -n" where sudo
        # needs no password, which must not happen here
        self.write_script(self.bin / "sudo", """\
            #!/bin/sh
            exit 1
            """)

    @staticmethod
    def write_script(path, source):
        path.write_text(textwrap.dedent(source))
        path.chmod(0o755)
        return path

    def write_dirs_file(self, content):
        (self.build / DIRS_FILE).write_text(content)

    def add_test(self, name, source, directory="tests"):
        path = self.tests / directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(textwrap.dedent(source))
        return path

    def run(self, **variables):
        """Runs the runner in the tree; returns its result, with its output in both streams in "output\""""
        env = {
            "CI": "1",
            "HOME": os.environ.get("HOME", str(self.root)),
            "PATH": str(self.bin) + os.pathsep + os.environ.get("PATH", os.defpath),
            "TMPDIR": str(self.tmp),
            "QORE_BINARY": str(self.build / "qore"),
            "LIBQORE_BINARY": str(self.build / ("libqore.so" if not self.qore else LIBQORE.name)),
            "CORE_DIR": str(self.cores),
            "TREE": str(self.root),
            "QORE_TEST_DIRS": "tests",
            "QORE_TEST_SOURCE_MODULES": "1",
        }
        if self.qore:
            env["LD_LIBRARY_PATH"] = str(BUILD_DIR)
            env["DYLD_LIBRARY_PATH"] = str(BUILD_DIR)
        env.update(variables)
        result = subprocess.run(["sh", str(RUN_TESTS)], cwd=self.root, env=env, capture_output=True, text=True,
                                timeout=300, preexec_fn=no_core_dumps)
        result.output = result.stdout + result.stderr
        return result


RECORD = """\
    import os
    with open(os.path.join(os.environ["TREE"], "module-path"), "w") as f:
        f.write(os.environ.get("QORE_MODULE_DIR", ""))
    """


class ModulePathTest(unittest.TestCase):
    """The module path that run_tests.sh gives a test"""

    def run_record(self, dirs_file=None, **variables):
        with tempfile.TemporaryDirectory(prefix="qore-runner-module-dirs-") as directory:
            tree = RunnerTree(directory)
            if dirs_file is not None:
                tree.write_dirs_file(dirs_file)
            tree.add_test("record.qtest", RECORD)
            result = tree.run(**variables)
            self.assertEqual(0, result.returncode, result.output)
            return (tree.root / "module-path").read_text().split(":"), str(tree.root.resolve())

    def test_external_dirs_come_last(self):
        path, root = self.run_record("/ext/one:/ext/two\n")
        self.assertEqual(["/ext/one", "/ext/two"], path[-2:], path)
        self.assertIn(root + "/qlib", path, path)
        self.assertLess(path.index(root + "/qlib"), path.index("/ext/one"), path)
        self.assertNotIn("", path, path)

    def test_external_dirs_follow_the_module_dir_of_the_environment(self):
        path, root = self.run_record("/ext/one\n", QORE_MODULE_DIR="/user/modules")
        self.assertEqual(["/user/modules", "/ext/one"], path[-2:], path)
        self.assertLess(path.index(root + "/qlib"), path.index("/user/modules"), path)

    def test_external_dirs_are_listed_once(self):
        path, _ = self.run_record("/ext/one\n")
        self.assertEqual(1, path.count("/ext/one"), path)

    # the path ends with an empty entry when QORE_MODULE_DIR is not set, as it did before the external directories
    def test_empty_file_adds_nothing(self):
        path, root = self.run_record("\n")
        self.assertEqual(root + "/qlib", [entry for entry in path if entry][-1], path)

    def test_module_dir_only_adds_nothing(self):
        # the caller gives the complete module path
        path, root = self.run_record("/ext/one\n", QORE_MODULE_DIR="/user/modules", QORE_MODULE_DIR_ONLY="1")
        self.assertNotIn("/ext/one", path, path)
        self.assertEqual("/user/modules", path[-1], path)

    def test_empty_module_dir_only_adds_nothing(self):
        # qore reads QORE_MODULE_DIR_ONLY as set even when it is empty
        path, root = self.run_record("/ext/one\n", QORE_MODULE_DIR="/user/modules", QORE_MODULE_DIR_ONLY="")
        self.assertNotIn("/ext/one", path, path)

    def test_no_file_adds_nothing(self):
        path, root = self.run_record()
        self.assertEqual(root + "/qlib", [entry for entry in path if entry][-1], path)


@unittest.skipUnless(HAVE_QORE, "no qore built in " + str(BUILD_DIR))
class BuildQoreTest(unittest.TestCase):
    """A qore test finds a module that is only in an external module directory of the build"""

    def test_module_only_in_external_dir_loads(self):
        with tempfile.TemporaryDirectory(prefix="qore-runner-module-dirs-") as directory:
            tree = RunnerTree(directory, qore=True)
            external = tree.root / "external"
            external.mkdir()
            (external / "ExternalOnly5708.qm").write_text(textwrap.dedent("""\
                %new-style
                module ExternalOnly5708 {
                    version = "1.0";
                    desc = "a module only in an external module directory";
                    author = "Qore Technologies, s.r.o.";
                    url = "https://qore.org";
                    license = "MIT";
                }
                public namespace ExternalOnly5708 {
                    public string sub value() {
                        return "external-5708";
                    }
                }
                """))
            tree.write_dirs_file(str(external.resolve()) + "\n")
            tree.add_test("external.qtest", """\
                #!/usr/bin/env qore
                %modern
                %requires ExternalOnly5708
                printf("value: %s\\n", ExternalOnly5708::value());
                """)
            result = tree.run(QORE_TEST_OPTS="--enable-debug")
            self.assertEqual(0, result.returncode, result.output)
            self.assertIn("value: external-5708", result.output)


    @unittest.skipUnless(HAVE_QCC, "no qcc built in " + str(BUILD_DIR))
    def test_qmod_compiled_against_external_module_loads(self):
        # a qmod of the build that requires a module in an external directory, as the qlib qmods require msgpack or
        # xml: it loads, and not its source (BINARY-MODULE-SOURCE-FALLBACK)
        with tempfile.TemporaryDirectory(prefix="qore-runner-module-dirs-") as directory:
            tree = RunnerTree(directory, qore=True)
            external = tree.root / "external"
            external.mkdir()
            (external / "ExternalDep5708.qm").write_text(textwrap.dedent("""\
                %new-style
                module ExternalDep5708 {
                    version = "1.0";
                    desc = "a module only in an external module directory";
                    author = "Qore Technologies, s.r.o.";
                    url = "https://qore.org";
                    license = "MIT";
                }
                public namespace ExternalDep5708 {
                    public string sub value() {
                        return "external-dep-5708";
                    }
                }
                """))
            source = tree.root / "src" / "BuildQmod5708.qm"
            source.parent.mkdir()
            source.write_text(textwrap.dedent("""\
                %new-style
                %requires ExternalDep5708
                module BuildQmod5708 {
                    version = "1.0";
                    desc = "a qmod of the build that requires a module in an external directory";
                    author = "Qore Technologies, s.r.o.";
                    url = "https://qore.org";
                    license = "MIT";
                }
                public namespace BuildQmod5708 {
                    public string sub value() {
                        return "qmod:" + ExternalDep5708::value();
                    }
                }
                """))
            qmod_dir = tree.build / "qlib-qmod"
            qmod_dir.mkdir()
            compile_env = dict(os.environ, LD_LIBRARY_PATH=str(BUILD_DIR), DYLD_LIBRARY_PATH=str(BUILD_DIR),
                               QORE_MODULE_DIR=str(external), QORE_MODULE_DIR_ONLY="1")
            compiled = subprocess.run([str(BUILD_DIR / "qcc"), "-m", "-O0", "-o", str(qmod_dir / "BuildQmod5708.qmod"),
                                       str(source)], env=compile_env, capture_output=True, text=True, timeout=300)
            self.assertEqual(0, compiled.returncode, compiled.stdout + compiled.stderr)
            # the source is not where the qore of the tests searches: only the qmod can load
            tree.write_dirs_file(str(external.resolve()) + "\n")
            tree.add_test("qmod.qtest", """\
                #!/usr/bin/env qore
                %modern
                %requires BuildQmod5708
                printf("value: %s\\n", BuildQmod5708::value());
                printf("file: %s\\n", get_module_hash().BuildQmod5708.filename);
                """)
            result = tree.run(QORE_TEST_OPTS="--enable-debug", QORE_TEST_SOURCE_MODULES="0")
            self.assertEqual(0, result.returncode, result.output)
            self.assertIn("value: qmod:external-dep-5708", result.output)
            self.assertIn("file: " + str((qmod_dir / "BuildQmod5708.qmod").resolve()), result.output)
            self.assertNotIn("BINARY-MODULE-SOURCE-FALLBACK", result.output)


if __name__ == "__main__":
    unittest.main()
