#!/usr/bin/env python3
"""run_tests.sh runs QORE_TEST_JOBS tests at a time, in lanes, and prints the output of each test as a whole.

The tests run run_tests.sh on a tree of stand-in tests: the qore stand-in runs a test file as a shell script, so each
test does what the case needs.  Tests that must run at the same time meet at FIFOs (each blocks until the other one
opens its end), so a run that does not run them at the same time never ends, and the case fails on its time limit;
nothing waits for a time.  The cases check the output (each test's output whole, between its header and its footer,
and the summary), the exit status (the number of failed tests), the lanes (one serial test at a time beside the
others, an exclusive test alone), the order (longest first), a test's time limit, and the processes a test leaves.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]

# runs the test file given (the argument ending in .qtest) as a shell script
QORE_STANDIN = """#!/bin/sh
for a in "$@"; do
    case "$a" in
        *.qtest) exec sh "$a" ;;
    esac
done
exit 2
"""


class ParallelRunnerTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="qore-runner-parallel-")
        self.root = Path(self.directory.name)
        self.tests = self.root / "examples/test"
        (self.tests / "docker_test").mkdir(parents=True)
        (self.tests / "docker_test/ci-job-processes.sh").symlink_to(
            ROOT / "examples/test/docker_test/ci-job-processes.sh")
        self.bin = self.root / "bin"
        self.bin.mkdir()
        (self.bin / "libqore.so").touch()
        qore = self.bin / "qore"
        qore.write_text(QORE_STANDIN)
        qore.chmod(0o755)
        self.shared = self.root / "shared"
        self.shared.mkdir()

    def tearDown(self):
        self.directory.cleanup()

    def fifo(self, name):
        path = self.shared / name
        os.mkfifo(path)
        return path

    def make_test(self, name, script, header=""):
        path = self.tests / name
        path.write_text("#!/bin/sh\n" + header + script)
        return path

    def run_runner(self, jobs, extra=None, timeout=60, path=None):
        env = {
            "PATH": path or (str(self.bin) + os.pathsep + os.defpath),
            "QORE_BINARY": str(self.bin / "qore"),
            "LIBQORE_BINARY": str(self.bin / "libqore.so"),
            "CORE_DIR": str(self.root / "cores"),
            "SHARED": str(self.shared),
            "QORE_TEST_JOBS": str(jobs),
        }
        env.update(extra or {})
        return subprocess.run(["sh", str(ROOT / "run_tests.sh")], cwd=self.root, env=env, capture_output=True,
                              text=True, timeout=timeout)

    def blocks(self, output):
        """Returns {test name: the lines of its block} and checks that each block is whole"""
        blocks = {}
        current = None
        for line in output.splitlines():
            match = re.match(r"Running test \((\d+)/(\d+)\): \./examples/test/(\S+)$", line)
            if match:
                self.assertIsNone(current, "a block starts before %s ends" % current)
                current = match.group(3)
                blocks[current] = []
                continue
            match = re.match(r"Test time: \./examples/test/(\S+): (\d+)s$", line)
            if match:
                self.assertEqual(current, match.group(1), "the footer of the block's test")
                current = None
                continue
            if current:
                blocks[current].append(line)
                # a line that a test prints names the test
                if line.startswith("out "):
                    self.assertEqual("out " + current, line.split(":")[0], "a line of another test in the block")
        self.assertIsNone(current)
        return blocks

    def test_tests_run_at_the_same_time_and_print_whole(self):
        # a and b meet: each blocks until the other one runs
        self.fifo("ab")
        self.make_test("a.qtest", 'echo "out a.qtest: 1"\ncat "$SHARED/ab" > /dev/null\necho "out a.qtest: 2"\n')
        self.make_test("b.qtest", 'echo "out b.qtest: 1"\necho met > "$SHARED/ab"\necho "out b.qtest: 2"\n'
                                  'echo "out b.qtest: err" >&2\n')
        self.make_test("c.qtest", 'echo "out c.qtest: 1"\nexit 3\n')
        self.make_test("d.qtest", 'echo "out d.qtest: 1"\n')
        result = self.run_runner(2)
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        blocks = self.blocks(result.stdout)
        self.assertEqual({"a.qtest", "b.qtest", "c.qtest", "d.qtest"}, set(blocks))
        for name in blocks:
            outs = [line for line in blocks[name] if line.startswith("out ")]
            expected = {"a.qtest": ["out a.qtest: 1", "out a.qtest: 2"],
                        "b.qtest": ["out b.qtest: 1", "out b.qtest: 2", "out b.qtest: err"],
                        "c.qtest": ["out c.qtest: 1"], "d.qtest": ["out d.qtest: 1"]}[name]
            self.assertEqual(expected, outs, name)
        self.assertIn("Running 4 tests, 2 at a time (0 serial, 0 exclusive)", result.stdout)
        self.assertIn("TESTING RESULT: Failure.", result.stdout)
        self.assertIn("Passed 3 out of 4 tests. 1 tests failed.", result.stdout)
        self.assertTrue(result.stdout.rstrip().endswith("Failed tests:\n./examples/test/c.qtest\n"
                                                        "*************************************"), result.stdout)

    def test_exit_status_is_the_number_of_failed_tests(self):
        for i in range(5):
            self.make_test("t%d.qtest" % i, "exit %d\n" % (i % 2))
        result = self.run_runner(3)
        self.assertEqual(2, result.returncode, result.stdout + result.stderr)
        failed = result.stdout.split("Failed tests:\n", 1)[1].split("\n*")[0].split()
        self.assertEqual(["./examples/test/t1.qtest", "./examples/test/t3.qtest"], failed, "in the order of names")
        # every test passes
        for i in range(5):
            self.make_test("t%d.qtest" % i, "exit 0\n")
        result = self.run_runner(3)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn("Passed 5 out of 5 tests. 0 tests failed.", result.stdout)

    def test_serial_lane(self):
        # one serial test at a time: each takes a lock that another serial test would find taken; a serial test
        # runs beside the others: s1 meets the free test f
        serial = "# test-parallel: serial - shares the stand-in's lock\n"
        lock = ('mkdir "$SHARED/lock" || { echo "out $T: the lock is taken"; exit 1; }\n'
                '%s\nrmdir "$SHARED/lock"\n')
        self.fifo("s1f")
        self.make_test("s1.qtest", lock % 'cat "$SHARED/s1f" > /dev/null', serial)
        self.make_test("s2.qtest", lock % "sleep 0", serial)
        self.make_test("s3.qtest", lock % "sleep 0", serial)
        self.make_test("f.qtest", 'echo met > "$SHARED/s1f"\n')
        self.make_test("g.qtest", "exit 0\n")
        result = self.run_runner(3)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn("Running 5 tests, 3 at a time (3 serial, 0 exclusive)", result.stdout)

    def test_exclusive_tests_run_alone(self):
        # an exclusive test finds no other test running; the others meet, so they run at the same time
        exclusive = "# test-parallel: exclusive - counts the tests that run\n"
        running = ('touch "$SHARED/running.$$"\n%s\nrm -f "$SHARED/running.$$"\n')
        alone = ('n=$(ls "$SHARED" | grep -c "^running\\.")\n'
                 '[ "$n" = 1 ] || { echo "out x: $n tests run"; exit 1; }\n')
        self.fifo("ab")
        self.make_test("a.qtest", running % 'cat "$SHARED/ab" > /dev/null')
        self.make_test("b.qtest", running % 'echo met > "$SHARED/ab"')
        self.make_test("x1.qtest", running % alone, exclusive)
        self.make_test("x2.qtest", running % alone, exclusive)
        self.make_test("c.qtest", running % "sleep 0")
        result = self.run_runner(3)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn("(0 serial, 2 exclusive)", result.stdout)
        # the exclusive tests run first
        order = re.findall(r"Running test \(\d+/\d+\): \./examples/test/(\S+)", result.stdout)
        self.assertEqual({"x1.qtest", "x2.qtest"}, set(order[:2]))

    def test_longest_first(self):
        names = ["a.qtest", "b.qtest", "c.qtest", "d.qtest"]
        for name in names:
            # each test records that it started
            self.make_test(name, 'echo "%s" >> "$SHARED/order"\n' % name)
        (self.tests / "docker_test/test-durations.tsv").write_text(
            "test\ttest-ubuntu-amd64\nexamples/test/a.qtest\t1\nexamples/test/b.qtest\t40\n"
            "examples/test/c.qtest\t5\nexamples/test/d.qtest\t30\n")
        result = self.run_runner(2, {"QORE_TEST_EXTRA_DIRS": ""})
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        started = (self.shared / "order").read_text().split()
        # b and d start first (in either order), then c and a as slots free up
        self.assertEqual({"b.qtest", "d.qtest"}, set(started[:2]))
        self.assertEqual({"a.qtest", "c.qtest"}, set(started[2:]))
        # the headers keep the numbers of the tests in the order of their names
        self.assertIn("Running test (2/4): ./examples/test/b.qtest", result.stdout)

    def test_shell_that_dies_is_a_failure(self):
        # the shell that runs a test is killed (here by the test itself): the run counts the test as failed and goes
        # on, instead of waiting for its result
        self.make_test("dies.qtest", "kill -9 $PPID\n")
        self.make_test("ok.qtest", "exit 0\n")
        result = self.run_runner(2)
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertIn("*** the shell that ran ./examples/test/dies.qtest ended without its result ***", result.stdout)
        self.assertIn("Passed 1 out of 2 tests. 1 tests failed.", result.stdout)
        self.assertIn("Failed tests:\n./examples/test/dies.qtest\n", result.stdout)

    def test_jobs_setting(self):
        self.make_test("a.qtest", "exit 0\n")
        self.make_test("b.qtest", "exit 0\n")
        result = self.run_runner("auto")
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertRegex(result.stdout, r"Running 2 tests, 2 at a time")
        for value in ("x", "-1", "2.5"):
            with self.subTest(value=value):
                result = self.run_runner(value)
                self.assertEqual(1, result.returncode)
                self.assertIn("QORE_TEST_JOBS is a number of tests or \"auto\"", result.stderr)
        # one test at a time, also when the variable is empty, prints as before: no parallel run
        for value in (1, ""):
            with self.subTest(value=value):
                result = self.run_runner(value)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertEqual("", result.stderr)
                self.assertNotIn("at a time", result.stdout)
                self.assertEqual(2, len(self.blocks(result.stdout)))
                self.assertIn("Passed 2 out of 2 tests. 0 tests failed.", result.stdout)

    def test_time_limit_in_a_parallel_run(self):
        timeout = shutil.which("timeout") or shutil.which("gtimeout")
        if not timeout:
            self.skipTest("needs timeout or gtimeout")
        self.fifo("never")
        # the test blocks until its limit: nothing writes to the FIFO; the other test still runs and passes
        self.make_test("hang.qtest", 'echo "out hang.qtest: started"\ncat "$SHARED/never"\n', "# test-timeout: 1\n")
        self.make_test("ok.qtest", "exit 0\n")
        path = str(self.bin) + os.pathsep + os.path.dirname(timeout) + os.pathsep + os.defpath
        result = self.run_runner(2, {"QORE_TEST_STACK_DUMP_WAIT": "1"}, path=path)
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        blocks = self.blocks(result.stdout)
        self.assertIn("out hang.qtest: started", blocks["hang.qtest"])
        self.assertIn("TIMEOUT: test exceeded 1s limit", blocks["hang.qtest"])
        self.assertIn("*** the test exceeded its 1s limit: SIGUSR1 sent for the call stacks of its threads ***",
                      blocks["hang.qtest"])
        self.assertIn("Failed tests:\n./examples/test/hang.qtest\n", result.stdout)

    @unittest.skipUnless(sys.platform.startswith("linux"), "the runner looks for leftover processes in /proc")
    def test_leftover_processes_in_a_parallel_run(self):
        # a process that the test leaves with an environment of its own (no QORE_TEST_PROCESS_MARK) still holds the
        # test's output, and is found by it
        self.make_test("leaves.qtest", "env -i /bin/sleep 300 &\necho \"out leaves.qtest: left\"\n")
        self.make_test("ok.qtest", "exit 0\n")
        result = self.run_runner(2)
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        blocks = self.blocks(result.stdout)
        self.assertIn("*** FAILURE: the test left processes running; they are killed: ***", blocks["leaves.qtest"])
        self.assertTrue(any("sleep 300" in line for line in blocks["leaves.qtest"]), blocks["leaves.qtest"])
        self.assertEqual(["./examples/test/leaves.qtest"],
                         result.stdout.split("Failed tests:\n", 1)[1].split("\n*")[0].split())

    def test_lane_declarations(self):
        # the lanes that tests declare are "serial" or "exclusive", with a reason
        declaration = re.compile(r"(?m)^#\s*test-parallel:.*$")
        valid = re.compile(r"^# test-parallel: (serial|exclusive) - \S.*$")
        found = 0
        for test in sorted((ROOT / "examples/test").rglob("*.qtest")) + sorted((ROOT / "modules").glob(
                "*/test/**/*.qtest")):
            for line in declaration.findall(test.read_text(errors="replace")):
                found += 1
                self.assertRegex(line, valid, "%s: '# test-parallel: serial|exclusive - <why>'" % test)
        self.assertGreater(found, 0)


if __name__ == "__main__":
    unittest.main()
