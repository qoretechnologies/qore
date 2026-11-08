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
import re
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

    def run(self, timeout=120, **variables):
        """Runs the runner in the tree; returns its result, with its output in both streams in "output\""""
        result = self.run_shell('exec sh "$RUN_TESTS"', timeout, **variables)
        result.output = result.stdout + result.stderr
        return result

    def break_fifo(self):
        """Makes the event FIFO of the runner fail: another runner removes it between this runner's mkfifo and its
        open, as runners that shared their FIFO did; the open then makes a regular file"""
        real = shutil.which("mkfifo")
        self.write_script("mkfifo", f"""\
            #!/bin/sh
            '{real}' "$@" || exit
            rm -f "$@"
            """)


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


@unittest.skipUnless(HAVE_TIMEOUT, "run_tests.sh runs a test without its watchdog without a timeout command")
class FailedFifoTest(unittest.TestCase):
    """A watchdog whose event FIFO fails reports it; it does not take it for the time limit"""

    def test_a_fifo_that_is_gone_when_opened_is_a_watchdog_error_and_not_a_timeout(self):
        # The open made a regular file, whose read ended at once, and the watchdog took that for the time limit: it
        # reported the test as having exceeded its 600 s limit and sent it SIGUSR1 at once.  Now the runner sees that
        # the file it opened is not a FIFO, reports it, and runs the test with the timeout command instead.
        with tempfile.TemporaryDirectory(prefix="qore-runner-watchdog-") as directory:
            tree = RunnerTree(directory)
            tree.break_fifo()
            tree.add_test("passes.qtest", """\
                # test-timeout: 60
                import time
                print("test started", flush=True)
                time.sleep(2)
                print("test ran to its end", flush=True)
                """)
            result = tree.run()
            self.assertIn("test ran to its end", result.output)
            self.assertIn("WATCHDOG ERROR: the watchdog's event FIFO", result.output)
            self.assertIn("was removed before it was opened, and the open made a file that is not a FIFO: the test is"
                          " run with a plain time limit", result.output)
            self.assertNotIn("TIMEOUT", result.output)
            self.assertNotIn("exceeded", result.output)
            # the test passed, but the run fails as a harness error
            self.assertRegex(result.output, r"Passed 1 out of +1 tests\. 0 tests failed\.")
            self.assertIn("Harness errors: the watchdog of 1 tests failed", result.output)
            self.assertIn("TESTING RESULT: Failure.", result.output)
            self.assertEqual(1, result.returncode, result.output)

    def test_a_test_is_still_ended_at_its_limit_when_the_fifo_has_failed(self):
        with tempfile.TemporaryDirectory(prefix="qore-runner-watchdog-") as directory:
            tree = RunnerTree(directory)
            tree.break_fifo()
            tree.add_test("hangs.qtest", """\
                # test-timeout: 3
                import time
                time.sleep(2)
                print("still running after 2 s", flush=True)
                time.sleep(600)
                """)
            # a second test runs after it as usual
            tree.add_test("passes.qtest", """\
                print("second test ran", flush=True)
                """)
            result = tree.run()
            self.assertIn("WATCHDOG ERROR: the watchdog's event FIFO", result.output)
            self.assertIn("still running after 2 s", result.output)
            self.assertIn("TIMEOUT: test exceeded 3s limit", result.output)
            self.assertIn("second test ran", result.output)
            self.assertRegex(result.output, r"Passed 1 out of +2 tests\. 1 tests failed\.")
            self.assertIn("Harness errors: the watchdog of 2 tests failed", result.output)
            self.assertEqual(2, result.returncode, result.output)


def watchdog_functions():
    """Returns the source of read_test_event() and run_with_watchdog() in the runner tested"""
    source = RUN_TESTS.read_text()
    functions = []
    for name in ("read_test_event", "run_with_watchdog"):
        match = re.search(r"^" + name + r"\(\) \{\n.*?^\}\n", source, re.S | re.M)
        if match is None:
            raise AssertionError(f"{name}() is not in {RUN_TESTS}")
        functions.append(match.group(0))
    return "".join(functions)


@unittest.skipUnless(HAVE_TIMEOUT, "run_tests.sh runs a test without its watchdog without a timeout command")
class WatchdogReadTest(unittest.TestCase):
    """A read of the event FIFO that ends without an event, before the time limit, is a watchdog error

    It cannot happen once the runner has made sure that its descriptor is the FIFO (see FailedFifoTest), so these
    cases run the watchdog's functions from run_tests.sh, as the reproduction of the race did, with a read_test_event()
    of their own.
    """

    def run_watchdog(self, read_test_event, command, limit=30, signals_enabled=1):
        with tempfile.TemporaryDirectory(prefix="qore-runner-watchdog-") as directory:
            timeout = "timeout" if shutil.which("timeout") else "gtimeout"
            script = textwrap.dedent(f"""\
                TIMEOUT_CMD={timeout}
                TIMEOUT_SIGNAL_OPTS=""
                TEST_STACK_DUMP_WAIT=30
                TEST_TIMEOUT_SIGNAL=ABRT
                TEST_SIGNALS_ENABLED={signals_enabled}
                TEST_EVENT_FIFO="$PWD/events"
                """) + watchdog_functions() + textwrap.dedent(read_test_event) + textwrap.dedent(f"""\
                run_with_watchdog {limit} {command}
                echo "run_with_watchdog: $? WATCHDOG_TEST_EXIT=$WATCHDOG_TEST_EXIT" \\
                    "WATCHDOG_TIMED_OUT=$WATCHDOG_TIMED_OUT WATCHDOG_ERROR=$WATCHDOG_ERROR"
                """)
            result = subprocess.run(["sh", "-c", script], cwd=directory, capture_output=True, text=True, timeout=120,
                                    preexec_fn=no_core_dumps)
            result.output = result.stdout + result.stderr
            return result

    def test_a_read_that_ends_before_the_limit_is_a_watchdog_error_and_the_test_is_not_ended(self):
        # a read of a file that ends, and a read that fails
        for name, read in (("end of file", "echo eof"), ("failed read", "return 2")):
            with self.subTest(read=name):
                result = self.run_watchdog(f"""\
                    read_test_event() {{
                        {read}
                    }}
                    """, "sh -c 'sleep 2; echo test ran to its end'")
                self.assertIn("WATCHDOG ERROR: the watchdog's read of its event FIFO ended without an event",
                              result.output)
                self.assertIn("test ran to its end", result.output)
                self.assertNotIn("exceeded", result.output)
                self.assertIn("run_with_watchdog: 0 WATCHDOG_TEST_EXIT=0 WATCHDOG_TIMED_OUT=0 WATCHDOG_ERROR=1",
                              result.output)

    def test_a_read_that_fails_after_the_limit_does_not_leave_the_test_running(self):
        # the time limit passes once the test ignores SIGUSR1 (it reports that through a FIFO of its own), then the read
        # for the call stacks fails: the test is ended at once, without waiting for the call stacks or for it to end
        result = self.run_watchdog("""\
            mkfifo ready
            read_test_event() {
                if [ ! -e limit-passed ]; then
                    : > limit-passed
                    read line < ready
                    return 124
                fi
                echo eof
            }
            """, f"'{sys.executable}' -c 'import signal, time; signal.signal(signal.SIGUSR1, signal.SIG_IGN); "
               "open(\"ready\", \"w\").write(\"ready\\n\"); time.sleep(60); print(\"test ran to its end\")'")
        self.assertIn("the test exceeded its 30s limit", result.output)
        self.assertIn("WATCHDOG ERROR: the watchdog's read of its event FIFO ended without an event", result.output)
        self.assertNotIn("test ran to its end", result.output)
        self.assertRegex(result.output, r"run_with_watchdog: 124 WATCHDOG_TEST_EXIT=134 WATCHDOG_TIMED_OUT=1 "
                                        r"WATCHDOG_ERROR=1")


if __name__ == "__main__":
    unittest.main()
