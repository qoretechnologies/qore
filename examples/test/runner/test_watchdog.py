#!/usr/bin/env python3
"""The watchdog of run_tests.sh: the event FIFO through which a test reports to it, and how it ends a test that
exceeds its time limit.

run_tests.sh runs a tree of its own with a stand-in for qore that runs each test file with Python, so a test is a
Python script that does what the case needs: it records what the runner gives it, reports events through the FIFO,
handles or blocks signals, or hangs.  The time limits are real, a few seconds each.  The runner tested is
QORE_RUN_TESTS_SH if set (to show that a case fails with an older run_tests.sh), else the one in this tree.  Core
dumps are disabled for the runner and the tests: its "ulimit -c unlimited" cannot raise the hard limit of 0.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import resource
import shutil
import stat
import subprocess
import sys
import tempfile
import textwrap
import unittest


ROOT = Path(__file__).resolve().parents[3]
RUN_TESTS = Path(os.environ.get("QORE_RUN_TESTS_SH", str(ROOT / "run_tests.sh"))).resolve()

# the commands run_tests.sh times a test with: GNU or BusyBox timeout, or GNU timeout as gtimeout (macOS)
HAVE_TIMEOUT = bool(shutil.which("timeout") or shutil.which("gtimeout"))


def no_core_dumps():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


class RunnerTree:
    """A tree that run_tests.sh runs in, with Python scripts as its tests"""

    def __init__(self, directory):
        self.root = Path(directory)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.tests = self.root / "examples/test"
        helpers = self.tests / "docker_test"
        helpers.mkdir(parents=True)
        # run_tests.sh reads its helpers from the tree it runs in
        (helpers / "ci-job-processes.sh").symlink_to(ROOT / "examples/test/docker_test/ci-job-processes.sh")
        self.cores = self.root / "cores"
        self.cores.mkdir()
        self.tmp = self.root / "tmp"
        self.tmp.mkdir()
        (self.bin / "libqore.so").touch()
        # qore: runs the test file it is given with Python, in the same process
        self.write_script("qore", f"""\
            #!/bin/sh
            for arg in "$@"; do
                case "$arg" in
                    *.qtest) exec '{sys.executable}' -B "$arg" ;;
                esac
            done
            echo "no test file in: $*" >&2
            exit 2
            """)
        # sudo: fails; with CI set, run_tests.sh sets the core file name of a macOS machine (kern.corefile, one setting
        # for the whole machine) with "sudo -n" where sudo needs no password, which must not happen here
        self.write_script("sudo", """\
            #!/bin/sh
            exit 1
            """)

    def write_script(self, name, source):
        path = self.bin / name
        path.write_text(textwrap.dedent(source))
        path.chmod(0o755)
        return path

    def add_test(self, name, source, directory="tests"):
        """Adds a test: a Python script with the given source"""
        path = self.tests / directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(textwrap.dedent(source))
        return path

    def env(self, **variables):
        env = {
            # CI: the runner does not set the machine's core pattern, which a root run outside CI would do
            "CI": "1",
            "HOME": os.environ.get("HOME", str(self.root)),
            "PATH": str(self.bin) + os.pathsep + os.environ.get("PATH", os.defpath),
            "TMPDIR": str(self.tmp),
            "QORE_BINARY": str(self.bin / "qore"),
            "LIBQORE_BINARY": str(self.bin / "libqore.so"),
            "CORE_DIR": str(self.cores),
            "TREE": str(self.root),
        }
        env.update(variables)
        return env

    def run_shell(self, script, timeout=120, **variables):
        """Runs a shell script in the tree; RUN_TESTS names the runner"""
        return subprocess.run(["sh", "-c", script], cwd=self.root, env=self.env(RUN_TESTS=str(RUN_TESTS), **variables),
                              capture_output=True, text=True, timeout=timeout, preexec_fn=no_core_dumps)


@unittest.skipUnless(HAVE_TIMEOUT, "run_tests.sh runs a test without its watchdog without a timeout command")
class EventFifoTest(unittest.TestCase):
    """The event FIFO of a runner is its own"""

    def test_runners_with_one_process_id_on_one_tree_have_fifos_of_their_own(self):
        # Runners in containers on one bind-mounted tree all had process ID 2, and so one FIFO, which they removed and
        # made again for each test: another runner's removal between this runner's mkfifo and its open made the open
        # create a regular file, at whose end the watchdog ended the test as having exceeded its time limit.  Here the
        # two runners are subshells of one shell, which have its process ID ($$), and they share the tree and the core
        # directory.  Each test records the FIFO it was given and whether it is one.  The runners write their output to
        # files: a runner takes a process other than itself and its ancestors that holds the output of the tests for
        # one that a test left running (Linux), and in a subshell $$ is not the runner but the shell, which holds the
        # output of this test's command.
        with tempfile.TemporaryDirectory(prefix="qore-runner-watchdog-") as directory:
            tree = RunnerTree(directory)
            record = """\
                import os
                import stat
                import sys
                fifo = os.environ["QORE_TEST_EVENT_FIFO"]
                with open(os.path.join(os.environ["TREE"], sys.argv[0].split(os.sep)[-2]), "w") as f:
                    f.write(fifo + "\\n" + str(stat.S_ISFIFO(os.stat(fifo).st_mode)) + "\\n")
                """
            tree.add_test("record.qtest", record, "a")
            tree.add_test("record.qtest", record, "b")
            result = tree.run_shell("""\
                QORE_TEST_DIRS=a; export QORE_TEST_DIRS; (. "$RUN_TESTS") > a.log 2>&1; echo "runner a: $?"
                QORE_TEST_DIRS=b; (. "$RUN_TESTS") > b.log 2>&1; echo "runner b: $?"
                """)
            output = (result.stdout + result.stderr + (tree.root / "a.log").read_text()
                      + (tree.root / "b.log").read_text())
            self.assertIn("runner a: 0", result.stdout, output)
            self.assertIn("runner b: 0", result.stdout, output)
            fifo_a, is_fifo_a = (tree.root / "a").read_text().splitlines()
            fifo_b, is_fifo_b = (tree.root / "b").read_text().splitlines()
            self.assertNotEqual(fifo_a, fifo_b, "two runners with one process ID on one tree share their event FIFO")
            for fifo, is_fifo in ((fifo_a, is_fifo_a), (fifo_b, is_fifo_b)):
                self.assertEqual("True", is_fifo, fifo)
                # in a directory of the runner's own in TMPDIR, which it removes when it ends; not in the core
                # directory, which other runners on the tree share
                directory = Path(fifo).parent
                self.assertEqual(tree.tmp, directory.parent, fifo)
                self.assertFalse(directory.exists(), fifo)
            self.assertEqual([], sorted(p.name for p in tree.cores.iterdir()),
                             "the runners leave the core directory empty")


if __name__ == "__main__":
    unittest.main()
