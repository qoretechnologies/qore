#!/usr/bin/env python3
"""The watchdog of run_tests.sh: the event FIFO through which a test reports to it, and how it ends a test that
exceeds its time limit.

run_tests.sh runs a tree of its own with a stand-in for qore that runs each test file with Python, so a test is a
Python script that does what the case needs: it records what the runner gives it, reports events through the FIFO,
handles or ignores signals, or hangs.  The time limits are real, a few seconds each.  The cases of SignalQoreTest run
qore tests with the qore of the build in QORE_TEST_BUILD_DIR, or in build/ in this tree, and are skipped without one.
The runner tested is QORE_RUN_TESTS_SH if set (to show that a case fails with an older run_tests.sh), else the one in
this tree.  Core dumps are disabled for the runner and the tests: its "ulimit -c unlimited" cannot raise the hard
limit of 0.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import errno
import fcntl
import os
from pathlib import Path
import re
import resource
import select
import shutil
import signal
import subprocess
import sys
import tempfile
import textwrap
import time
import unittest


ROOT = Path(__file__).resolve().parents[3]
RUN_TESTS = Path(os.environ.get("QORE_RUN_TESTS_SH", str(ROOT / "run_tests.sh"))).resolve()

# the commands run_tests.sh times a test with: GNU or BusyBox timeout, or GNU timeout as gtimeout (macOS)
HAVE_TIMEOUT = bool(shutil.which("timeout") or shutil.which("gtimeout"))

# the build whose qore runs the qore tests
BUILD_DIR = Path(os.environ.get("QORE_TEST_BUILD_DIR", str(ROOT / "build")))
LIBQORE = next((BUILD_DIR / name for name in ("libqore.so", "libqore.dylib") if (BUILD_DIR / name).exists()), None)
HAVE_QORE = (BUILD_DIR / "qore").exists() and LIBQORE is not None


def no_core_dumps():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


class RunnerTree:
    """A tree that run_tests.sh runs in, with Python scripts as its tests, or qore scripts with the qore of the build"""

    def __init__(self, directory, qore=False):
        self.qore = qore
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
        # the modules of the qore tests: QUnit from the source of this tree (QORE_TEST_SOURCE_MODULES below)
        (self.root / "qlib").symlink_to(ROOT / "qlib")
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
        if self.qore:
            env.update({
                "QORE_BINARY": str(BUILD_DIR / "qore"),
                "LIBQORE_BINARY": str(LIBQORE),
                "QORE_TEST_SOURCE_MODULES": "1",
            })
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

    @staticmethod
    def sections(output):
        """Returns the output of each test, by the name of its file"""
        sections = {}
        for part in output.split("Running test (")[1:]:
            name = part.split("\n", 1)[0].rsplit("/", 1)[-1]
            sections[name] = part.split("*************************************", 1)[0]
        return sections

    def break_fifo(self):
        """Makes the event FIFO of the runner fail: another runner removes it between this runner's mkfifo and its
        open, as runners that shared their FIFO did; the open then makes a regular file.  The other FIFOs of the runner
        (those of a parallel run and of its trap) are made as usual."""
        real = shutil.which("mkfifo")
        self.write_script("mkfifo", f"""\
            #!/bin/sh
            '{real}' "$@" || exit
            case "$*" in
                */events) rm -f "$@" ;;
            esac
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


@unittest.skipUnless(os.path.isdir("/proc/self/task"), "processes a test leaves are found through /proc (Linux)")
class SharedOutputTest(unittest.TestCase):
    """Runners that share their output (the shards of one job) are no survivors of each other's tests"""

    def test_a_runner_does_not_end_another_runner_that_shares_its_output(self):
        # A process that held the output of the tests was a survivor of the test that had just ended, unless it was
        # the runner or one of its ancestors: another runner writing to the same pipe was killed.  Runner b's test runs
        # while runner a's test ends and a looks for survivors, and only then ends.
        with tempfile.TemporaryDirectory(prefix="qore-runner-watchdog-") as directory:
            tree = RunnerTree(directory)
            b_running = tree.root / "b-running"
            b_release = tree.root / "b-release"
            os.mkfifo(b_running)
            os.mkfifo(b_release)
            tree.add_test("waits.qtest", f"""\
                with open({str(b_running)!r}, "w") as f:
                    f.write("running\\n")
                open({str(b_release)!r}).read()
                print("test b ended", flush=True)
                """, "b")
            tree.add_test("ends.qtest", f"""\
                open({str(b_running)!r}).read()
                print("test a ended", flush=True)
                """, "a")
            # both runners write to this shell's output, a pipe
            result = tree.run_shell(f"""\
                QORE_TEST_DIRS=b sh "$RUN_TESTS" & b=$!
                QORE_TEST_DIRS=a sh "$RUN_TESTS"; echo "runner a: $?"
                echo released 1<> {str(b_release)!r}
                wait $b; echo "runner b: $?"
                """)
            output = result.stdout + result.stderr
            self.assertIn("test a ended", output)
            self.assertIn("test b ended", output)
            self.assertIn("runner a: 0", output)
            self.assertIn("runner b: 0", output)
            self.assertNotIn("left processes running", output)


class InterruptedRunnerTest(unittest.TestCase):
    """A runner ended by a signal ends the test that runs, its watchdog and the processes the test left, removes its
    files, the directory of its event FIFO among them, and ends by the signal"""

    def interrupt(self, sig, test_source="", wait=None, test_lines=(), second_signal=False, jobs=1):
        """Runs a runner whose test blocks, sends the runner the signal and checks that it ends as it should

        test_source: lines of Python (a string or a list) that the test runs first, with {ready} and {alive} for the
        descriptors of the harness's pipes; second_signal: the runner is sent the signal again once it reports that
        its trap waits for the test, and every process of the run must have ended when the runner has; jobs: the
        number of tests the runner runs at the same time (QORE_TEST_JOBS), each of them a test that blocks; the signal
        is sent once all of them run, and each line of test_lines must be in the output once for each test
        """
        if isinstance(test_source, str):
            test_source = [test_source]
        with tempfile.TemporaryDirectory(prefix="qore-runner-watchdog-") as directory:
            tree = RunnerTree(directory)
            # every process of the run inherits the write ends of these pipes, at descriptors the runner does not use
            # (it uses 4 and 5): the ready pipe carries the test's message, or is at its end if the run ends before the
            # test writes it; the alive pipe is at its end when the last process of the run has ended (the runner, its
            # watchdog, the test and what the test started)
            ready_r = alive_r = None
            write_ends = []
            runner = None
            try:
                ready_r, ready_w = os.pipe()
                alive_r, alive_w = os.pipe()
                for fd in (ready_w, alive_w):
                    write_ends.append(fcntl.fcntl(fd, fcntl.F_DUPFD, 50))
                    os.close(fd)
                ready_fd, alive_fd = write_ends
                # the test tells the harness that it runs (the runner has made its files and started its watchdog by
                # then) and blocks on a FIFO that only the harness's cleanup writes
                hold = tree.root / "hold"
                os.mkfifo(hold)
                for number in range(1, jobs + 1):
                    tree.add_test("blocks%d.qtest" % number, f"""\
                        # test-timeout: 600
                        import os
                        {(chr(10) + " " * 24).join(line.format(ready=ready_fd, alive=alive_fd) for line in test_source)}
                        os.write({ready_fd}, b"running\\n")
                        open({str(hold)!r}).read()
                        """)
                env = tree.env()
                if jobs > 1:
                    env["QORE_TEST_JOBS"] = str(jobs)
                if wait is not None:
                    env["QORE_TEST_INTERRUPT_WAIT"] = str(wait)
                trap_report = None
                if second_signal:
                    # the runner reports through it that its trap waits for the test; open for reading and writing,
                    # so that the harness's wait for the report is bounded by the select below, not by an open
                    report_path = tree.root / "trap-report"
                    os.mkfifo(report_path)
                    trap_report = os.open(report_path, os.O_RDWR | os.O_NONBLOCK)
                    env["QORE_TEST_RUNNER_TRAP_REPORT"] = str(report_path)
                # the output goes to a file, so that a process left holding it cannot keep the harness waiting: the
                # alive pipe reports it
                output_path = tree.root / "runner.out"
                with open(output_path, "wb") as output_file:
                    runner = subprocess.Popen(["sh", str(RUN_TESTS)], cwd=tree.root, env=env, stdout=output_file,
                                              stderr=subprocess.STDOUT, pass_fds=tuple(write_ends),
                                              preexec_fn=no_core_dumps)
                for fd in write_ends:
                    os.close(fd)
                write_ends = []
                started = b""
                while started.count(b"\n") < jobs:
                    readable, _, _ = select.select([ready_r], [], [], 120)
                    self.assertEqual([ready_r], readable, "the tests did not start")
                    data = os.read(ready_r, 100)
                    self.assertNotEqual(b"", data, "the run ended before the tests started")
                    started += data
                self.assertEqual(b"running\n" * jobs, started)
                self.assertEqual(1, len(list(tree.tmp.glob("qore-test-events.*"))),
                                 "the runner made its FIFO directory")
                runner.send_signal(sig)
                if second_signal:
                    # each test's shell reports it (a parallel run)
                    reports = b""
                    while reports.count(b"\n") < jobs:
                        readable, _, _ = select.select([trap_report], [], [], 60)
                        self.assertEqual([trap_report], readable, "the runner did not report that its trap waits")
                        reports += os.read(trap_report, 100)
                    self.assertEqual(b"waiting\n" * jobs, reports)
                    runner.send_signal(sig)
                runner.wait(timeout=120)
                output = output_path.read_text()
                # the runner ends by the signal, so that its caller sees why
                self.assertEqual(-sig, runner.returncode, output)
                for line in test_lines:
                    self.assertEqual(jobs, output.count(line), output)
                if jobs > 1:
                    # the output of each test that was ended is printed
                    for number in range(1, jobs + 1):
                        self.assertIn("blocks%d.qtest" % number, output)
                # the read returns when no process holds the pipe anymore; one that is left keeps it open, which the
                # time limit of the select ends as a failure
                # (a runner that has waited for every process it started ends after them: then the pipe is at its end
                # at once)
                readable, _, _ = select.select([alive_r], [], [], 0 if second_signal else 60)
                self.assertEqual([alive_r], readable, "a process of the run is left after signal %d" % sig)
                self.assertEqual(b"", os.read(alive_r, 1), "a process of the run is left after signal %d" % sig)
            finally:
                if runner is not None and runner.poll() is None:
                    runner.kill()
                    runner.wait()
                # a test that is left (after a failure) is released from its FIFO, if it waits there
                try:
                    os.close(os.open(tree.root / "hold", os.O_WRONLY | os.O_NONBLOCK))
                except OSError as e:
                    if e.errno not in (errno.ENXIO, errno.ENOENT):
                        raise
                for fd in write_ends + [ready_r, alive_r, trap_report if second_signal else None]:
                    if fd is not None:
                        os.close(fd)
            self.assertEqual([], list(tree.tmp.glob("qore-test-events.*")),
                             "the runner ended by signal %d left its FIFO directory" % sig)
            self.assertEqual([], list(tree.tmp.glob("qore-run-tests.*")),
                             "the runner ended by signal %d left the directory of its parallel run" % sig)
            self.assertEqual([], sorted(p.name for p in tree.cores.iterdir()),
                             "the runner ended by signal %d left its files in the core directory" % sig)

    def test_sighup(self):
        self.interrupt(signal.SIGHUP)

    def test_sigint(self):
        self.interrupt(signal.SIGINT)

    def test_sigterm(self):
        self.interrupt(signal.SIGTERM)

    @unittest.skipUnless(HAVE_TIMEOUT, "the runner bounds its wait for the test only with a timeout command")
    def test_a_test_that_handles_the_signal_and_hangs_is_killed(self):
        # it ignores SIGTERM: the runner kills it after its wait
        self.interrupt(signal.SIGTERM, "import signal; signal.signal(signal.SIGTERM, signal.SIG_IGN)", 1,
                       ("the run is interrupted, and the test did not end within 1s: SIGKILL sent",))

    @unittest.skipUnless(HAVE_TIMEOUT, "the runner bounds its wait for the test only with a timeout command")
    def test_a_second_signal_while_the_trap_runs_is_ignored(self):
        # The second signal re-entered the trap, which started a second helper and lost the first one: that one held
        # the output until its wait ended and then signalled the test's old process ID.  The test ignores the first
        # SIGTERM and ends at a second one, which it must not get (the runner ignores the second signal): it is
        # killed after the wait, and when the runner has ended, every process of the run has ended.  This proves the
        # fix only where sh is dash (Linux CI): bash defers the second signal until the trap has ended, so the trap is
        # not re-entered there with or without the fix.
        self.interrupt(signal.SIGTERM, [
            "import signal",
            "terms = []",
            "signal.signal(signal.SIGTERM, lambda *args: os._exit(0) if terms else terms.append(1))",
        ], 1, ("the run is interrupted, and the test did not end within 1s: SIGKILL sent",), second_signal=True)

    @unittest.skipUnless(os.path.isdir("/proc/self/task"), "processes a test leaves are found through /proc (Linux)")
    def test_the_processes_a_test_left_are_ended(self):
        # the test starts a process that outlives it (in a session of its own, so that no signal to the test's
        # process group reaches it), which inherits the test's mark and the harness's pipes
        self.interrupt(signal.SIGTERM, "import subprocess; subprocess.Popen(['sleep', '600'], start_new_session=True, "
                       "pass_fds=({ready}, {alive}))")

    # A parallel run (QORE_TEST_JOBS) ended by a signal ends each test that runs as a run of one test at a time does:
    # its traps were "exit", which left the tests, their watchdogs and their shells running, and the event FIFO
    # directory behind.  Its shells run in the background, where SIGINT cannot be trapped, so the runner sends them
    # SIGTERM for it.
    def test_sighup_in_a_parallel_run(self):
        self.interrupt(signal.SIGHUP, jobs=2)

    def test_sigint_in_a_parallel_run(self):
        self.interrupt(signal.SIGINT, jobs=2)

    def test_sigterm_in_a_parallel_run(self):
        self.interrupt(signal.SIGTERM, jobs=2)

    @unittest.skipUnless(HAVE_TIMEOUT, "the runner bounds its wait for the test only with a timeout command")
    def test_tests_that_handle_the_signal_and_hang_in_a_parallel_run_are_killed(self):
        self.interrupt(signal.SIGTERM, "import signal; signal.signal(signal.SIGTERM, signal.SIG_IGN)", 1,
                       ("the run is interrupted, and the test did not end within 1s: SIGKILL sent",), jobs=2)

    @unittest.skipUnless(HAVE_TIMEOUT, "the runner bounds its wait for the test only with a timeout command")
    def test_a_second_signal_while_the_trap_of_a_parallel_run_runs_is_ignored(self):
        self.interrupt(signal.SIGTERM, [
            "import signal",
            "terms = []",
            "signal.signal(signal.SIGTERM, lambda *args: os._exit(0) if terms else terms.append(1))",
        ], 1, ("the run is interrupted, and the test did not end within 1s: SIGKILL sent",), second_signal=True,
            jobs=2)

    @unittest.skipUnless(os.path.isdir("/proc/self/task"), "processes a test leaves are found through /proc (Linux)")
    def test_the_processes_the_tests_of_a_parallel_run_left_are_ended(self):
        self.interrupt(signal.SIGTERM, "import subprocess; subprocess.Popen(['sleep', '600'], start_new_session=True, "
                       "pass_fds=({ready}, {alive}))", jobs=2)


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
            # the test blocks on a FIFO that nothing writes, so it can only end by its time limit; that the limit is
            # kept, not cut short, is shown by the time the run took (no earlier end is possible), not by a margin
            hold = tree.root / "hold"
            os.mkfifo(hold)
            tree.add_test("hangs.qtest", f"""\
                # test-timeout: 3
                print("test started", flush=True)
                open({str(hold)!r}).read()
                """)
            # a second test runs after it as usual
            tree.add_test("passes.qtest", """\
                print("second test ran", flush=True)
                """)
            start = time.monotonic()
            result = tree.run()
            elapsed = time.monotonic() - start
            self.assertIn("WATCHDOG ERROR: the watchdog's event FIFO", result.output)
            self.assertIn("test started", result.output)
            self.assertGreaterEqual(elapsed, 3, "the test was ended before its limit: " + result.output)
            self.assertIn("TIMEOUT: test exceeded 3s limit", result.output)
            self.assertIn("second test ran", result.output)
            self.assertRegex(result.output, r"Passed 1 out of +2 tests\. 1 tests failed\.")
            self.assertIn("Harness errors: the watchdog of 2 tests failed", result.output)
            self.assertEqual(2, result.returncode, result.output)

    def test_a_watchdog_error_in_a_parallel_run_fails_the_run(self):
        # each test of a parallel run runs in a shell of its own, which counted the watchdog's error only there: the
        # run passed
        with tempfile.TemporaryDirectory(prefix="qore-runner-watchdog-") as directory:
            tree = RunnerTree(directory)
            tree.break_fifo()
            for name in ("a.qtest", "b.qtest"):
                tree.add_test(name, f"""\
                    # test-timeout: 60
                    print("test {name} ran to its end", flush=True)
                    """)
            result = tree.run(QORE_TEST_JOBS="2")
            self.assertIn("2 at a time", result.output)
            self.assertEqual(2, result.output.count("WATCHDOG ERROR: the watchdog's event FIFO"), result.output)
            self.assertIn("test a.qtest ran to its end", result.output)
            self.assertIn("test b.qtest ran to its end", result.output)
            self.assertRegex(result.output, r"Passed 2 out of +2 tests\. 0 tests failed\.")
            self.assertIn("Harness errors: the watchdog of 2 tests failed (WATCHDOG ERROR in their output):\n"
                          "./examples/test/tests/a.qtest\n./examples/test/tests/b.qtest\n", result.output)
            self.assertIn("TESTING RESULT: Failure.", result.output)
            self.assertEqual(2, result.returncode, result.output)


def runner_function(name):
    """Returns the source of a function of the runner tested"""
    match = re.search(r"^" + name + r"\(\) \{\n.*?^\}\n", RUN_TESTS.read_text(), re.S | re.M)
    if match is None:
        raise AssertionError(f"{name}() is not in {RUN_TESTS}")
    return match.group(0)


class RunnerDescendantsTest(unittest.TestCase):
    """The processes that descend from a runner are found even when a process ends during the scan"""

    def test_a_process_that_ends_during_the_scan_is_skipped(self):
        # awk read the status files as its input files and ended at the first one that was gone, and printed nothing,
        # which disabled the rule that finds the processes that hold the output of the tests.  A /proc of its own
        # (QORE_TEST_PROC) has a process whose status file is gone, as when it ends between the glob and the read.
        with tempfile.TemporaryDirectory(prefix="qore-runner-proc-") as directory:
            proc = Path(directory)
            for pid, ppid in ((100, 1), (101, 100), (103, 101), (104, 1)):
                (proc / str(pid)).mkdir()
                (proc / str(pid) / "status").write_text(f"Name:\tp{pid}\nPPid:\t{ppid}\n")
            (proc / "102").mkdir()
            (proc / "102" / "status").symlink_to(proc / "gone")
            result = subprocess.run(["sh", "-c", runner_function("runner_descendants") + "runner_descendants 100"],
                                    env=dict(os.environ, QORE_TEST_PROC=str(proc)), capture_output=True, text=True,
                                    timeout=60)
            self.assertEqual(["101", "103"], sorted(result.stdout.split()), result.stdout + result.stderr)


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


# A test that hangs until it is ended; it reports through the event FIFO and handles SIGUSR1 as its parameters say:
# REPORT is the process ID it reports as having the handler ("" for none, "self" for its own), HANDLE is "handler"
# (prints "call stacks printed" and reports "dumped"), "ignore" or "default", and MASKS is a list of the
# (SigBlk, SigCgt) masks of its threads in a /proc tree of its own (QORE_TEST_PROC), made before it reports
HANGING_TEST = """\
    # test-timeout: 3
    import os
    import signal
    import time
    REPORT = {report!r}
    HANDLE = {handle!r}
    MASKS = {masks!r}
    fifo = os.environ["QORE_TEST_EVENT_FIFO"]

    def report(event):
        fd = os.open(fifo, os.O_WRONLY | os.O_NONBLOCK)
        os.write(fd, (event + "\\n").encode())
        os.close(fd)

    def stacks(sig, frame):
        print("call stacks printed", flush=True)
        report("dumped")

    if HANDLE == "handler":
        signal.signal(signal.SIGUSR1, stacks)
    elif HANDLE == "ignore":
        signal.signal(signal.SIGUSR1, signal.SIG_IGN)
    if MASKS is not None:
        task = os.path.join(os.environ["QORE_TEST_PROC"], str(os.getpid()), "task")
        os.makedirs(task)
        for i, (blocked, caught) in enumerate(MASKS):
            os.makedirs(os.path.join(task, str(i + 1)))
            with open(os.path.join(task, str(i + 1), "status"), "w") as f:
                f.write("Name:\\tpython\\nSigQ:\\t0/1000\\nSigPnd:\\t0000000000000000\\nShdPnd:\\t0000000000000000\\n"
                        "SigBlk:\\t%s\\nSigIgn:\\t0000000000000000\\nSigCgt:\\t%s\\n" % (blocked, caught))
    if REPORT:
        report("handler %d" % (os.getpid() if REPORT == "self" else int(REPORT)))
    print("test hangs", flush=True)
    while True:
        time.sleep(600)
"""

# the masks of a thread that blocks or catches SIGUSR1, and of one that does neither
USR1 = format(1 << (signal.SIGUSR1 - 1), "016x")
NONE = "0" * 16


@unittest.skipUnless(HAVE_TIMEOUT, "run_tests.sh runs a test without its watchdog without a timeout command")
class SignalTest(unittest.TestCase):
    """A test that exceeds its time limit is sent SIGUSR1 only when the signal cannot end it; otherwise it is aborted
    at once (SIGABRT), so that it dumps core

    SIGUSR1 ended a test that did not handle it, with no core dump and no call stacks, as one that hung before QUnit had
    installed its handler.
    """

    def run_tests(self, tests, proc=None, **variables):
        """Runs the given tests ({file: (report, handle, masks)}); returns the output of each test and the result"""
        with tempfile.TemporaryDirectory(prefix="qore-runner-watchdog-") as directory:
            tree = RunnerTree(directory)
            for name, (report, handle, masks) in tests.items():
                tree.add_test(name, HANGING_TEST.format(report=report, handle=handle, masks=masks))
            if proc is not None:
                # a /proc tree of the tests' own: empty, so the thread masks of no process can be read, unless a test
                # makes its own
                (tree.root / "proc").mkdir()
                variables["QORE_TEST_PROC"] = str(tree.root / "proc")
            variables.setdefault("QORE_TEST_STACK_DUMP_WAIT", "60")
            result = tree.run(**variables)
            sections = RunnerTree.sections(result.output)
            self.assertEqual(sorted(tests), sorted(sections), result.output)
            return sections, result

    def assert_usr1(self, output, stacks=True):
        self.assertIn("the test exceeded its 3s limit: SIGUSR1 sent for the call stacks of its threads", output)
        if stacks:
            self.assertIn("call stacks printed", output)
        self.assertIn("TIMEOUT: test exceeded 3s limit (it was ended by signal 6)", output)

    def assert_aborted_at_once(self, output):
        self.assertIn("test hangs", output)
        self.assertIn("the test exceeded its 3s limit: SIGABRT sent at once, as it does not handle SIGUSR1", output)
        self.assertNotIn("SIGUSR1 sent", output)
        self.assertIn("TIMEOUT: test exceeded 3s limit (it was ended by signal 6)", output)

    def test_a_test_without_a_handler_is_aborted_at_once(self):
        # with the thread masks of /proc where it exists (Linux), and without them, where QUnit's report decides
        for proc in (None, "empty"):
            with self.subTest(proc=proc):
                sections, result = self.run_tests({"hangs.qtest": ("", "default", None)}, proc=proc)
                self.assert_aborted_at_once(sections["hangs.qtest"])
                self.assertEqual(1, result.returncode, result.output)

    def test_a_test_with_a_handler_is_sent_sigusr1_and_prints_its_call_stacks(self):
        # it reports its handler, and catches the signal (/proc/<pid>/task/*/status on Linux); the call stacks are
        # printed, the test is aborted when it reports them, without waiting for QORE_TEST_STACK_DUMP_WAIT
        for proc in (None, "empty"):
            with self.subTest(proc=proc):
                sections, result = self.run_tests({"handler.qtest": ("self", "handler", None)}, proc=proc)
                self.assert_usr1(sections["handler.qtest"])

    def test_the_report_of_the_test_s_own_process_decides(self):
        # a Qore handler does not show in the signal masks of the threads: Qore's signal thread waits for the signal
        # with sigwait(), and Linux shows it as unblocked in that thread meanwhile
        child = os.getpid()
        sections, result = self.run_tests({
            "reported.qtest": ("self", "ignore", None),
            # the masks do not matter when the handler is reported
            "reported-not-blocked.qtest": ("self", "ignore", [(NONE, NONE)]),
            # a process that the test started has the FIFO in its environment as well
            "reported-by-another-process.qtest": (str(child), "ignore", None),
            "not-reported.qtest": ("", "ignore", None),
        }, proc="empty", QORE_TEST_STACK_DUMP_WAIT="1")
        for name in ("reported.qtest", "reported-not-blocked.qtest"):
            with self.subTest(test=name):
                self.assert_usr1(sections[name], stacks=False)
        for name in ("reported-by-another-process.qtest", "not-reported.qtest"):
            with self.subTest(test=name):
                self.assert_aborted_at_once(sections[name])

    def test_on_linux_a_test_that_catches_or_blocks_the_signal_in_every_thread_is_sent_it(self):
        # the masks in the /proc tree that each test makes for itself, which run_tests.sh reads wherever the test's
        # task directory exists in it; a test that ignores SIGUSR1 survives it, and is aborted after
        # QORE_TEST_STACK_DUMP_WAIT
        sections, result = self.run_tests({
            "all-block.qtest": ("", "ignore", [(USR1, NONE), (USR1, NONE)]),
            "caught.qtest": ("", "ignore", [(NONE, USR1), (NONE, USR1)]),
            "one-blocks.qtest": ("", "ignore", [(USR1, NONE), (NONE, NONE)]),
            "no-threads.qtest": ("", "ignore", []),
        }, proc="empty", QORE_TEST_STACK_DUMP_WAIT="1")
        for name in ("all-block.qtest", "caught.qtest"):
            with self.subTest(test=name):
                self.assert_usr1(sections[name], stacks=False)
        for name in ("one-blocks.qtest", "no-threads.qtest"):
            with self.subTest(test=name):
                self.assert_aborted_at_once(sections[name])

    def test_signal_handling_disabled(self):
        sections, result = self.run_tests({"handler.qtest": ("self", "handler", [(USR1, USR1)])}, proc="empty",
                                          QORE_TEST_OPTS="-b")
        output = sections["handler.qtest"]
        self.assertIn("the test exceeded its 3s limit: SIGABRT sent (signal handling is disabled)", output)
        self.assertNotIn("SIGUSR1 sent", output)
        self.assertNotIn("call stacks printed", output)
        self.assertIn("TIMEOUT: test exceeded 3s limit (it was ended by signal 6)", output)


@unittest.skipUnless(HAVE_TIMEOUT, "run_tests.sh runs a test without its watchdog without a timeout command")
@unittest.skipUnless(HAVE_QORE, f"no qore build in {BUILD_DIR}")
class SignalQoreTest(unittest.TestCase):
    """The same with qore tests, and QUnit from this tree"""

    QUNIT_TEST = """\
        #!/usr/bin/env qore
        # test-timeout: 5
        %modern
        %requires QUnit
        %exec-class HangTest
        class HangTest inherits QUnit::Test {
            constructor() : Test("HangTest", "1.0") {
                addTestCase("hangs", \\hangsInQueueGet());
                set_return_value(main());
            }
            hangsInQueueGet() {
                Queue q();
                q.get();
            }
        }
        """

    # hangs before it creates its QUnit test object, so it has no SIGUSR1 handler
    PLAIN_TEST = """\
        #!/usr/bin/env qore
        # test-timeout: 5
        %modern
        Queue q();
        q.get();
        """

    def run_tests(self, tests, **variables):
        with tempfile.TemporaryDirectory(prefix="qore-runner-watchdog-") as directory:
            tree = RunnerTree(directory, qore=True)
            for name, source in tests.items():
                tree.add_test(name, source)
            result = tree.run(QORE_TEST_STACK_DUMP_WAIT="60", **variables)
            sections = RunnerTree.sections(result.output)
            self.assertEqual(sorted(tests), sorted(sections), result.output)
            return sections

    def test_qunit_test_that_hangs_prints_its_call_stacks_and_one_that_hangs_before_it_is_aborted(self):
        sections = self.run_tests({"qunit.qtest": self.QUNIT_TEST, "plain.qtest": self.PLAIN_TEST})
        output = sections["qunit.qtest"]
        self.assertIn("the test exceeded its 5s limit: SIGUSR1 sent for the call stacks of its threads", output)
        self.assertIn("=== QUnit: SIGUSR1: call stacks of all threads ===", output)
        self.assertIn("HangTest::hangsInQueueGet", output)
        self.assertIn("TIMEOUT: test exceeded 5s limit (it was ended by signal 6)", output)
        output = sections["plain.qtest"]
        self.assertIn("the test exceeded its 5s limit: SIGABRT sent at once, as it does not handle SIGUSR1", output)
        self.assertNotIn("SIGUSR1 sent", output)
        self.assertIn("TIMEOUT: test exceeded 5s limit (it was ended by signal 6)", output)

    def test_qunit_test_with_signal_handling_disabled_is_aborted_at_once(self):
        sections = self.run_tests({"qunit.qtest": self.QUNIT_TEST}, QORE_TEST_OPTS="-b")
        output = sections["qunit.qtest"]
        self.assertIn("the test exceeded its 5s limit: SIGABRT sent (signal handling is disabled)", output)
        self.assertNotIn("SIGUSR1 sent", output)
        self.assertNotIn("=== QUnit: SIGUSR1", output)
        self.assertIn("TIMEOUT: test exceeded 5s limit (it was ended by signal 6)", output)


if __name__ == "__main__":
    unittest.main()
