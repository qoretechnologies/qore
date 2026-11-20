#!/usr/bin/env python3
"""The native call stacks of the processes that a test started, which run_tests.sh prints when the test exceeds its
time limit (issue #6888).

A hang in a process that a test started showed only as a timeout: the runner printed the Qore call stacks of the test
(SIGUSR1) and read its core, nothing of its children.  Here run_tests.sh runs a tree of its own (see
test_watchdog.RunnerTree), whose test is a Python script that starts children which block on a latch (a read of a FIFO
that nobody writes) and waits for them; it exceeds its time limit (LIMIT), and the runner must print the stacks of each
child, labelled with its process ID and command line, before it signals the test.  The stack tools are the real ones
(gdb or eu-stack on Linux, sample or lldb on macOS), or stand-ins on PATH that fail, print, or block on a latch, to
show that a capture that hangs is ended at its limit and that a missing tool is reported.

The runner tested is QORE_RUN_TESTS_SH if set (to show that a case fails with an older run_tests.sh), else the one in
this tree.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_watchdog import HAVE_TIMEOUT, RunnerTree  # noqa: E402


# the tools that run_tests.sh tries by default
DEFAULT_TOOLS = ["sample", "lldb"] if sys.platform == "darwin" else ["gdb", "eu-stack"]

# A child that blocks on its latch: a read of a FIFO that it holds open for writing as well, so that the read never
# ends; it reports its process ID first through the test's output
CHILD = """\
import os
import sys
latch = os.open(sys.argv[1], os.O_RDWR)
print("child %d started" % os.getpid(), flush=True)
os.read(latch, 1)
"""

# The time limit of the tests: long enough for a test to have started its children (Python processes) under load
LIMIT = 5

# A test that starts {children} children (CHILD, in stuck_child.py next to it) and waits for them, so it exceeds its
# time limit; it does not handle SIGUSR1, so the runner aborts it at once (SIGABRT) after the captures
TEST = """\
# test-timeout: {limit}
import os
import subprocess
import sys
here = os.path.dirname(os.path.abspath(sys.argv[0]))
latch = os.path.join(here, "latch")
children = [subprocess.Popen([sys.executable, "-B", os.path.join(here, "stuck_child.py"), latch])
            for i in range({children})]
for child in children:
    child.wait()
"""

# the frame of a read(): gdb's "read ()" or "__GI___libc_read", eu-stack's "read", lldb's "`read + 8", sample's
# "read  (in libsystem_kernel.dylib)" or "__read_nocancel"; not "Thread" or "pthread_..."
READ_FRAME = re.compile(r"(?<![A-Za-z])read(_nocancel)?\b")


def ptrace_attach_works(tool):
    """Returns True if the given debugger can attach to a process that it does not descend from (a sibling), as the
    runner's watchdog does: yama's ptrace_scope 1, or a container without CAP_SYS_PTRACE, forbids it on Linux"""
    if tool not in ("gdb", "eu-stack"):
        return True
    target = subprocess.Popen([sys.executable, "-c", "import sys; sys.stdin.read()"], stdin=subprocess.PIPE)
    try:
        args = ["gdb", "-nx", "-batch", "-p", str(target.pid), "-ex", "bt"] if tool == "gdb" \
            else ["eu-stack", "-p", str(target.pid)]
        result = subprocess.run(args, stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=60,
                                env=dict(os.environ, DEBUGINFOD_URLS=""))
        return result.returncode == 0 and "Operation not permitted" not in result.stdout + result.stderr
    finally:
        target.stdin.close()
        target.wait()


@unittest.skipUnless(HAVE_TIMEOUT, "run_tests.sh runs a test without its watchdog without a timeout command")
class DescendantStacksTest(unittest.TestCase):

    def run_test(self, children=1, jobs=1, scripts=None, harness_limit=120, **variables):
        """Runs TEST with the given number of children; returns the test's output, the children's process IDs, the
        runner's result, the seconds the run took and the tree's root (for the files of the stand-ins)

        scripts: stand-in commands put on PATH, {name: source}, with {latch} for a FIFO that nobody writes and {calls}
        for a file to which a stand-in may append
        """
        directory = tempfile.TemporaryDirectory(prefix="qore-runner-stacks-")
        self.addCleanup(directory.cleanup)
        tree = RunnerTree(directory.name)
        test = tree.add_test("hangs.qtest", TEST.format(children=children, limit=LIMIT))
        (test.parent / "stuck_child.py").write_text(CHILD)
        os.mkfifo(test.parent / "latch")
        tool_latch = tree.root / "tool-latch"
        os.mkfifo(tool_latch)
        calls = tree.root / "calls"
        for name, source in (scripts or {}).items():
            tree.write_script(name, source.format(latch=tool_latch, calls=calls))
        if jobs > 1:
            variables["QORE_TEST_JOBS"] = str(jobs)
        start = time.monotonic()
        result = tree.run(timeout=harness_limit, **variables)
        elapsed = time.monotonic() - start
        sections = RunnerTree.sections(result.output)
        self.assertEqual(["hangs.qtest"], sorted(sections), result.output)
        output = sections["hangs.qtest"]
        pids = [int(pid) for pid in re.findall(r"^child (\d+) started$", output, re.M)]
        self.assertEqual(children, len(pids), output)
        self.assertIn("TIMEOUT: test exceeded %ds limit" % LIMIT, output)
        self.assertEqual(1, result.returncode, result.output)
        # the children were ended with the test's process group
        for pid in pids:
            self.assertFalse(self.process_runs(pid), "child %d was left running" % pid)
        return output, pids, result, elapsed, tree.root

    @staticmethod
    def process_runs(pid):
        try:
            os.kill(pid, 0)
        except ProcessLookupError:
            return False
        # a zombie that nobody waits for counts as ended
        state = subprocess.run(["ps", "-o", "stat=", "-p", str(pid)], capture_output=True, text=True).stdout
        return bool(state.strip()) and not state.strip().startswith("Z")

    def capture(self, output, pid):
        """Returns the lines of the capture of the given process, after its label (which must be there)"""
        label = re.search(r"^\*\*\* native call stacks of process %d \(a descendant of test (\d+)\): (.*) \*\*\*$"
                          % pid, output, re.M)
        self.assertIsNotNone(label, output)
        self.assertIn("stuck_child.py", label.group(2), output)
        rest = output[label.end():]
        end = re.search(r"^\*\*\* (native call stacks of process \d+|end of the native call stacks)", rest, re.M)
        self.assertIsNotNone(end, output)
        return rest[:end.start()]

    def assert_before_signals(self, output):
        """The captures end before the test is sent any signal"""
        end = output.find("*** end of the native call stacks of the processes of test")
        self.assertNotEqual(-1, end, output)
        signal_line = output.find("SIGABRT sent at once")
        self.assertNotEqual(-1, signal_line, output)
        self.assertLess(end, signal_line, output)

    def check_real_tool(self, jobs):
        tools = [tool for tool in DEFAULT_TOOLS if shutil.which(tool)]
        if not tools:
            self.skipTest("none of %s is installed" % ", ".join(DEFAULT_TOOLS))
        if not ptrace_attach_works(tools[0]):
            self.skipTest("%s cannot attach to a process it does not descend from (ptrace_scope or "
                          "CAP_SYS_PTRACE)" % tools[0])
        output, pids, result, elapsed, root = self.run_test(children=1, jobs=jobs)
        stacks = self.capture(output, pids[0])
        self.assertRegex(stacks, READ_FRAME, output)
        self.assertNotIn("failed (exit code", stacks, output)
        self.assertNotIn("was killed", stacks, output)
        self.assert_before_signals(output)

    def test_the_stacks_of_a_child_stuck_on_a_latch_are_printed(self):
        self.check_real_tool(jobs=1)

    def test_the_stacks_of_a_child_stuck_on_a_latch_are_printed_in_a_parallel_run(self):
        # the output of a test of a parallel run goes to its log, which the runner prints when the test ends
        self.check_real_tool(jobs=2)

    def test_a_capture_that_hangs_is_killed_at_its_limit(self):
        # every default tool is a stand-in that blocks on a latch; the first one is killed at the limit of one capture,
        # and no other is tried for the process.  A debugger blocks only when it attaches to a process (-p; sample is
        # given only a process ID): the runner also runs gdb on a core that it takes for the test's, and in a serial
        # run that is any new core in the kernel's core directory, such as one of another program of the machine
        blocks = ('#!/bin/sh\ncase "$0 $*" in *sample*|*" -p "*) ;; *) exit 0 ;; esac\n'
                  'echo "$0 $$" >> "{calls}"\nread line < "{latch}"\n')
        output, pids, result, elapsed, root = self.run_test(
            children=1, scripts={tool: blocks for tool in DEFAULT_TOOLS}, harness_limit=60,
            QORE_TEST_STACK_CAPTURE_WAIT="2")
        stacks = self.capture(output, pids[0])
        self.assertIn("*** process %d: %s did not end within 2s and was killed ***" % (pids[0], DEFAULT_TOOLS[0]),
                      stacks)
        calls = (root / "calls").read_text().splitlines()
        self.assertEqual(1, len(calls), calls)
        self.assertEqual(DEFAULT_TOOLS[0], os.path.basename(calls[0].split()[0]), calls)
        # the stand-in was killed, not left blocking
        self.assertFalse(self.process_runs(int(calls[0].split()[-1])), calls)
        self.assert_before_signals(output)
        # the test's limit, the 2 s capture and the ends of the test (none of which waits for a time), well inside the
        # 30 s of the default QORE_TEST_STACK_CAPTURE_WAIT
        self.assertLess(elapsed, LIMIT + 25, output)

    def test_the_captures_of_a_test_are_bounded_together(self):
        # the first child's capture hangs until the limit of all of them; the second child is not captured
        blocks = '#!/bin/sh\necho "$0 $$" >> "{calls}"\nread line < "{latch}"\n'
        output, pids, result, elapsed, root = self.run_test(
            children=2, scripts={"blocking-tool": blocks}, harness_limit=60, QORE_TEST_STACK_TOOLS="blocking-tool",
            QORE_TEST_STACK_CAPTURE_WAIT="60", QORE_TEST_STACK_CAPTURE_TOTAL="2")
        pids.sort()
        self.assertIn("blocking-tool did not end within 2s and was killed", self.capture(output, pids[0]))
        self.assertIn("*** process %d: not captured, as the captures have taken their 2s "
                      "(QORE_TEST_STACK_CAPTURE_TOTAL) ***" % pids[1], self.capture(output, pids[1]))
        self.assertEqual(1, len((root / "calls").read_text().splitlines()))
        self.assertLess(elapsed, LIMIT + 25, output)

    def test_a_missing_tool_is_reported(self):
        output, pids, result, elapsed, root = self.run_test(QORE_TEST_STACK_TOOLS="qore-no-such-stack-tool")
        self.assertIn("cannot be captured: none of the tools (qore-no-such-stack-tool) is installed", output)
        # the process is still listed, with its command line
        self.assertEqual("", self.capture(output, pids[0]).strip(), output)
        self.assert_before_signals(output)

    def test_a_tool_that_fails_is_followed_by_the_next(self):
        scripts = {
            "failing-tool": '#!/bin/sh\necho "cannot attach to $1"\nexit 3\n',
            "printing-tool": '#!/bin/sh\necho "stacks of $1 from a stand-in"\n',
        }
        output, pids, result, elapsed, root = self.run_test(
            scripts=scripts, QORE_TEST_STACK_TOOLS="no-such-stack-tool failing-tool printing-tool")
        stacks = self.capture(output, pids[0])
        self.assertIn("cannot attach to %d" % pids[0], stacks)
        self.assertIn("*** process %d: failing-tool failed (exit code 3) ***" % pids[0], stacks)
        self.assertIn("stacks of %d from a stand-in" % pids[0], stacks)
        self.assertNotIn("no tool could capture", stacks)

    def test_every_tool_failing_is_reported(self):
        scripts = {"failing-tool": '#!/bin/sh\nexit 3\n'}
        output, pids, result, elapsed, root = self.run_test(scripts=scripts, QORE_TEST_STACK_TOOLS="failing-tool")
        stacks = self.capture(output, pids[0])
        self.assertIn("*** process %d: no tool could capture its call stacks ***" % pids[0], stacks)

    def test_a_test_without_children_has_no_captures(self):
        directory = tempfile.TemporaryDirectory(prefix="qore-runner-stacks-")
        self.addCleanup(directory.cleanup)
        tree = RunnerTree(directory.name)
        latch = tree.root / "latch"
        os.mkfifo(latch)
        tree.add_test("hangs.qtest", f"""\
            # test-timeout: 3
            import os
            os.read(os.open({str(latch)!r}, os.O_RDWR), 1)
            """)
        # a tool that would be run reports it
        tree.write_script("reporting-tool", '#!/bin/sh\necho "reporting-tool was run"\n')
        result = tree.run(QORE_TEST_STACK_TOOLS="reporting-tool")
        self.assertIn("TIMEOUT: test exceeded 3s limit", result.output)
        self.assertNotIn("native call stacks", result.output)
        self.assertNotIn("reporting-tool was run", result.output)


if __name__ == "__main__":
    unittest.main()
