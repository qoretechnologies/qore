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
import textwrap
import time
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_watchdog import HAVE_TIMEOUT, ROOT, RunnerTree, end_session, start_in_session  # noqa: E402

# the root helper of the CI jobs (see examples/test/docker_test/ci-stack-capture.sh)
HELPER = ROOT / "examples/test/docker_test/ci-stack-capture.sh"
LINUX = os.path.isdir("/proc/self")
# a Python built without its _ctypes extension has no ctypes: the test of a process whose main thread has ended calls
# prctl() and pthread_exit() through it, in a child run with this Python
try:
    import ctypes  # noqa: F401
    HAVE_CTYPES = True
except ImportError:
    HAVE_CTYPES = False


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
child = [sys.executable, "-B", os.path.join(here, "stuck_child.py"), latch]
if {detached}:
    # each child is started by a shell that ends at once, so that it is adopted by another process (init, launchd);
    # the test then blocks on the latch itself
    for i in range({children}):
        subprocess.run(["sh", "-c", '"$@" &', "sh"] + child, check=True)
    os.read(os.open(latch, os.O_RDWR), 1)
children = [subprocess.Popen(child) for i in range({children})]
for child in children:
    child.wait()
"""

# gdb 17's notice of an attach that ptrace refused (Ubuntu's GNU gdb 17.1, as the CI images have it), after which it
# exits with 0
GDB17_REFUSED = ("Could not attach to process.  If your uid matches the uid of the target\n"
                 "process, check the setting of /proc/sys/kernel/yama/ptrace_scope, or try\n"
                 "again as the root user.  For more details, see /etc/sysctl.d/10-ptrace.conf\n"
                 "ptrace: Inappropriate ioctl for device.")

# A program that opens the FIFO given (returning once a writer has opened it) and then stays until it is killed: the
# program that a process of the helper's cases executes, whose start its writer sees
STAYS = "import os, sys; os.open(sys.argv[1], os.O_RDONLY); r, w = os.pipe(); os.read(r, 1)"

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


class StacksCase(unittest.TestCase):
    """What the cases have in common: a run of TEST, and the parts of its output"""

    def run_test(self, children=1, jobs=1, scripts=None, harness_limit=120, detached=False, **variables):
        """Runs TEST with the given number of children (detached: adopted by another process); returns the test's
        output, the children's process IDs, the runner's result, the seconds the run took and the tree's root (for the
        files of the stand-ins)

        scripts: stand-in commands put on PATH, {name: source}, with {latch} for a FIFO that nobody writes and {calls}
        for a file to which a stand-in may append
        """
        directory = tempfile.TemporaryDirectory(prefix="qore-runner-stacks-")
        self.addCleanup(directory.cleanup)
        tree = RunnerTree(directory.name)
        test = tree.add_test("hangs.qtest", TEST.format(children=children, limit=LIMIT, detached=detached))
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
        result.root = tree.root
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
        label = re.search(r"^\*\*\* native call stacks of process %d \(a process of test (\d+)\): (.*) \*\*\*$"
                          % pid, output, re.M)
        self.assertIsNotNone(label, output)
        self.assertIn("stuck_child.py", label.group(2), output)
        self.assertFalse(label.group(2).endswith(" "), output)
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


@unittest.skipUnless(HAVE_TIMEOUT, "run_tests.sh runs a test without its watchdog without a timeout command")
class DescendantStacksTest(StacksCase):

    def check_real_tool(self, jobs=1, detached=False):
        tools = [tool for tool in DEFAULT_TOOLS if shutil.which(tool)]
        if not tools:
            self.skipTest("none of %s is installed" % ", ".join(DEFAULT_TOOLS))
        if not ptrace_attach_works(tools[0]):
            self.skipTest("%s cannot attach to a process it does not descend from (ptrace_scope or "
                          "CAP_SYS_PTRACE)" % tools[0])
        output, pids, result, elapsed, root = self.run_test(children=1, jobs=jobs, detached=detached)
        stacks = self.capture(output, pids[0])
        self.assertRegex(stacks, READ_FRAME, output)
        self.assertNotIn("failed (exit code", stacks, output)
        self.assertNotIn("was killed", stacks, output)
        self.assert_before_signals(output)

    def test_the_stacks_of_a_child_stuck_on_a_latch_are_printed(self):
        self.check_real_tool()

    def test_the_stacks_of_a_child_stuck_on_a_latch_are_printed_in_a_parallel_run(self):
        # the output of a test of a parallel run goes to its log, which the runner prints when the test ends
        self.check_real_tool(jobs=2)

    def test_the_stacks_of_a_child_adopted_by_another_process_are_printed(self):
        # a child whose parent (a shell that ran it with "&") ended is no descendant of the test: it is found by the
        # test's mark (Linux) or process group (elsewhere)
        self.check_real_tool(detached=True)

    def test_the_children_adopted_by_another_process_are_captured_with_any_tool(self):
        output, pids, result, elapsed, root = self.run_test(
            children=2, detached=True, scripts={"printing-tool": '#!/bin/sh\necho "stacks of $1 from a stand-in"\n'},
            QORE_TEST_STACK_TOOLS="printing-tool")
        for pid in pids:
            self.assertIn("stacks of %d from a stand-in" % pid, self.capture(output, pid))
        # the shells that started them have ended, and the test itself is not captured
        self.assertEqual(2, output.count("from a stand-in"), output)
        self.assert_before_signals(output)

    def test_times_that_are_not_whole_numbers_fall_back_to_the_defaults(self):
        # shell arithmetic on "2m" was an error that ended the watchdog, which never ended the test, and the run never
        # ended; "30s" dropped the limit of each capture
        blocks = '#!/bin/sh\necho "$0 $$" >> "{calls}"\nread line < "{latch}"\n'
        for wait, total, limit, messages in (
                ("30s", "2m", 30, ("QORE_TEST_STACK_CAPTURE_WAIT=30s is not a whole number of seconds from 1 to "
                                   "999999999: 30 is used",
                                   "QORE_TEST_STACK_CAPTURE_TOTAL=2m is not a whole number of seconds from 1 to "
                                   "999999999: 120 is used")),
                ("2", "1.5", 2, ("QORE_TEST_STACK_CAPTURE_TOTAL=1.5 is not a whole number of seconds from 1 to "
                                 "999999999: 120 is used",)),
                ("1000000000", "3", 3, ("QORE_TEST_STACK_CAPTURE_WAIT=1000000000 is not a whole number of seconds",)),
                ("0", "3", 3, ("QORE_TEST_STACK_CAPTURE_WAIT=0 is not",)),
                ("x", "03", 3, ("QORE_TEST_STACK_CAPTURE_WAIT=x is not",)),
                ("02", "-1", 2, ("QORE_TEST_STACK_CAPTURE_TOTAL=-1 is not",))):
            with self.subTest(wait=wait, total=total):
                output, pids, result, elapsed, root = self.run_test(
                    scripts={"blocking-tool": blocks}, harness_limit=120, QORE_TEST_STACK_TOOLS="blocking-tool",
                    QORE_TEST_STACK_CAPTURE_WAIT=wait, QORE_TEST_STACK_CAPTURE_TOTAL=total)
                for message in messages:
                    self.assertIn(message, result.output)
                self.assertIn("*** process %d: blocking-tool did not end within %ds and was killed ***"
                              % (pids[0], limit), self.capture(output, pids[0]))
                self.assertLess(elapsed, LIMIT + limit + 25, output)

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
        # the whole of the 2 s: the first capture's limit is taken with the clock reading of the end of the total, so a
        # second that begins in between does not shorten it
        self.assertIn("*** process %d: blocking-tool did not end within 2s and was killed ***" % pids[0],
                      self.capture(output, pids[0]))
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

    def test_an_attach_that_the_system_refuses_is_reported_once_and_not_tried_again(self):
        # Where the system refuses the attach (yama's ptrace_scope or the container on Linux, macOS's rules for lldb),
        # every tool reported it for every process, each with an error.  Now the first refusal is reported in one line,
        # no other process or tool is tried, and the test is ended as usual.  The stand-ins are the default tools: on
        # Linux, gdb reports EPERM as gdb does; on macOS, sample fails and lldb reports its refusal as lldb does.  A
        # debugger run on a core (no -p; see test_a_capture_that_hangs_is_killed_at_its_limit) does nothing.
        record = ('#!/bin/sh\ncase "$0 $*" in *sample*|*" -p "*) ;; *) exit 0 ;; esac\n'
                  'echo "${{0##*/}} $*" >> "{calls}"\n')
        if sys.platform == "darwin":
            scripts = {
                "sample": record + 'echo "sample cannot examine process $1"\nexit 1\n',
                "lldb": record + 'echo "error: attach failed: attach failed (Not allowed to attach to process.)"\n'
                                 'exit 1\n',
            }
            refusing = ["sample", "lldb"]
        else:
            scripts = {
                # gdb 17's notice and exit code: 0
                "gdb": record + "printf '%s\\n' '" + GDB17_REFUSED + "'\nexit 0\n",
                "eu-stack": record + 'echo "stacks from eu-stack"\n',
            }
            refusing = ["gdb"]
        # the limit of a capture is long, to show that the refusal is not waited for
        output, pids, result, elapsed, root = self.run_test(children=2, scripts=scripts, harness_limit=60,
                                                            QORE_TEST_STACK_CAPTURE_WAIT="60")
        pids.sort()
        lines = [line for line in output.splitlines() if "ptrace restricted" in line]
        self.assertEqual(1, len(lines), output)
        self.assertIn("no stacks captured (processes %d %d)" % tuple(pids), lines[0])
        if sys.platform == "darwin":
            self.assertIn("the system does not allow lldb to attach to process %d" % pids[0], lines[0])
        else:
            try:
                scope = Path("/proc/sys/kernel/yama/ptrace_scope").read_text().strip()
            except OSError:
                scope = "unset"
            self.assertIn("*** ptrace restricted: yama ptrace_scope=%s, " % scope, lines[0])
        # each tool was run once, on the first process only
        calls = (root / "calls").read_text().splitlines()
        self.assertEqual(refusing, [call.split()[0] for call in calls], calls)
        for call in calls:
            self.assertIn(str(pids[0]), call.split()[1:], calls)
        self.assertNotIn("native call stacks of process %d" % pids[1], output)
        self.assertNotIn("stacks from eu-stack", output)
        self.assertNotIn("failed (exit code 1)", output.split("ptrace restricted")[1])
        self.assertNotIn("no tool could capture", output)
        # then the test is ended as usual
        self.assertLess(output.index("ptrace restricted"), output.index("*** end of the native call stacks"))
        self.assert_before_signals(output)
        self.assertLess(elapsed, LIMIT + 25, output)

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


@unittest.skipUnless(HAVE_TIMEOUT and LINUX, "the scan of /proc and the CI helper are Linux's")
class LinuxScanAndHelperTest(StacksCase):
    """The limit of the scan of /proc, and the captures through the CI job's root helper (here run as the user of the
    harness, which the helper then answers without gosu)"""

    def start_helper(self, directory, env=None):
        """Starts the helper in a session of its own; returns its request FIFO.  Its tools are stand-ins in a bin
        directory of its own, which print that they ran in the helper"""
        directory = Path(directory)
        tools = directory / "helper-bin"
        tools.mkdir()
        for name in ("gdb", "eu-stack"):
            path = tools / name
            path.write_text('#!/bin/sh\necho "the helper ran %s $*"\n' % name)
            path.chmod(0o755)
        fifo = directory / "requests"
        os.mkfifo(fifo, 0o600)
        (directory / "work").mkdir(mode=0o700)
        helper_env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ.get("PATH", os.defpath))
        helper_env.update(env or {})
        self.helper_log = open(directory / "helper.log", "w+")
        self.addCleanup(self.helper_log.close)
        helper = start_in_session(["sh", str(HELPER), "serve", subprocess.run(
            ["id", "-un"], capture_output=True, text=True).stdout.strip(), str(directory)], env=helper_env,
            stdin=subprocess.DEVNULL, stdout=self.helper_log, stderr=subprocess.STDOUT)
        self.addCleanup(end_session, helper)
        # an open for writing returns once the helper has opened its FIFO (it holds it open from then on, so the writes
        # of send() never meet a FIFO without a reader); limited, for a helper that does not start
        subprocess.run(["timeout", "30", "sh", "-c", ': > "$1"', "sh", str(fifo)], check=True)
        return fifo

    def send(self, fifo, line):
        """Writes a line to the helper's FIFO; fails at once (ENXIO) if no helper reads it, as when a helper did not
        start, rather than block: a blocked write left the case and the processes it started running"""
        fd = os.open(fifo, os.O_WRONLY | os.O_NONBLOCK)
        try:
            os.write(fd, (line + "\n").encode())
        finally:
            os.close(fd)

    def ask(self, fifo, request, directory):
        """Sends the helper a request whose reply is a FIFO in the given directory; returns the answer"""
        reply = Path(directory) / ("reply%d" % time.monotonic_ns())
        os.mkfifo(reply, 0o600)
        self.send(fifo, request.replace("{reply}", str(reply)))
        # the helper opens the reply for writing once it answers; the read ends when it has written all of it
        result = subprocess.run(["timeout", "30", "cat", str(reply)], capture_output=True, text=True)
        return result.stdout

    def test_a_capture_runs_through_the_helper(self):
        with tempfile.TemporaryDirectory(prefix="qore-runner-helper-") as directory:
            fifo = self.start_helper(directory)
            output, pids, result, elapsed, root = self.run_test(
                children=2, detached=True, scripts={"gdb": '#!/bin/sh\necho "the runner ran gdb $*"\n'},
                QORE_TEST_STACK_CAPTURE_HELPER=str(fifo))
            for pid in pids:
                stacks = self.capture(output, pid)
                self.assertRegex(stacks, r"the helper ran gdb -nx -batch .* -p %d -ex info proc status "
                                         r"-ex thread apply all bt" % pid)
                self.assertNotIn("the runner ran", stacks)
                self.assertNotIn("qore-stack-capture-status", stacks)
            self.assert_before_signals(output)
            # the answers' FIFOs are gone
            self.assertEqual([], list(root.glob("tmp/qore-test-events.*/stack-reply*")))

    @staticmethod
    def owner(pid):
        try:
            return os.stat("/proc/%d" % pid).st_uid
        except OSError:
            return None

    def test_the_helper_captures_a_process_in_the_group_of_a_marked_one(self):
        # a test leads its process group; a process it started with an environment of its own (env -i) has no mark
        with tempfile.TemporaryDirectory(prefix="qore-runner-helper-") as directory:
            fifo = self.start_helper(directory)
            latch = Path(directory) / "latch"
            os.mkfifo(latch)
            # the leader carries the mark and starts an unmarked child in its group, then both block on the latch
            leader = start_in_session(["sh", "-c", 'env -i sh -c "echo \\$\\$; exec 3<>$1; read x <&3" sh "$1" & '
                                       'wait', "sh", str(latch)],
                                      env=dict(os.environ, QORE_TEST_PROCESS_MARK="77.7"), stdout=subprocess.PIPE,
                                      stderr=subprocess.DEVNULL, text=True)
            self.addCleanup(end_session, leader)
            self.addCleanup(leader.stdout.close)
            child = int(leader.stdout.readline())
            self.assertEqual(os.getpgid(leader.pid), os.getpgid(child))
            answer = self.ask(fifo, "%d 5 77.7 gdb {reply}" % child, directory)
            self.assertIn("the helper ran gdb ", answer)
            self.assertIn(" -p %d " % child, answer)
            self.assertTrue(answer.endswith("qore-stack-capture-status: 0\n"), answer)
            # another mark: refused
            answer = self.ask(fifo, "%d 5 77.8 gdb {reply}" % child, directory)
            self.assertIn("refused: process %d is not a process of the test with mark 77.8" % child, answer)

    def test_the_helper_takes_the_identity_before_its_checks(self):
        # The process executes another program right after the helper's last check (its mark), as one that executes a
        # set-user-ID program then would: the helper took its identity only after the checks, so it took the new
        # program's both before and after the capture, and sent that program's stacks.  The helper's grep (the check of
        # the mark) is a stand-in that releases the process from its first latch and returns once the new program has
        # opened the second one.
        with tempfile.TemporaryDirectory(prefix="qore-runner-helper-") as directory:
            first = Path(directory) / "first"
            second = Path(directory) / "second"
            os.mkfifo(first)
            os.mkfifo(second)
            fifo = self.start_helper(directory)
            grep = Path(directory) / "helper-bin" / "grep"
            grep.write_text('#!/bin/sh\ncase "$*" in\n    *QORE_TEST_PROCESS_MARK=77.6*)\n        cat > /dev/null\n'
                            '        echo released > "%s"\n        exec 3> "%s"\n        exit 0\n        ;;\nesac\n'
                            'exec \'%s\' "$@"\n' % (first, second, shutil.which("grep")))
            grep.chmod(0o755)
            target = start_in_session(
                ["sh", "-c", 'read x < "$1"; exec "$3" -c "$4" "$2"', "sh", str(first), str(second),
                 sys.executable, STAYS],
                env=dict(os.environ, QORE_TEST_PROCESS_MARK="77.6"), stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL)
            self.addCleanup(end_session, target)
            answer = self.ask(fifo, "%d 5 77.6 gdb {reply}" % target.pid, directory)
            self.assertEqual("qore-stack-capture: refused: process %d changed during the capture\n"
                             "qore-stack-capture-status: 126\n" % target.pid, answer)

    def test_the_helper_checks_the_user_of_the_process_stopped_for_the_capture(self):
        # gdb prints the process's status while it is stopped (where it cannot execute another program): a user ID that
        # is not the runner's, or group IDs that differ, keep its stacks from being sent; the status lines are not sent
        uid = os.geteuid()
        # the runner's group: the primary group of its user, which the helper takes (id -g)
        gid = int(subprocess.run(["id", "-g"], capture_output=True, text=True).stdout)
        none = "0000000000000000"
        some = "0000000000000400"
        for name, uids, gids, caps, sent in (
                ("runner", (uid,) * 4, (gid,) * 4, (none,) * 3, True),
                ("root", (0,) * 4, (gid,) * 4, (none,) * 3, False),
                ("set-user-ID", (uid, 0, 0, 0), (gid,) * 4, (none,) * 3, False),
                ("set-group-ID", (uid,) * 4, (gid, 0, 0, 0), (none,) * 3, False),
                ("another group", (uid,) * 4, (gid + 1,) * 4, (none,) * 3, False),
                ("permitted capabilities", (uid,) * 4, (gid,) * 4, (some, none, none), False),
                ("effective capabilities", (uid,) * 4, (gid,) * 4, (none, some, none), False),
                ("ambient capabilities", (uid,) * 4, (gid,) * 4, (none, none, some), False),
                ("no capabilities", (uid,) * 4, (gid,) * 4, None, False),
                ("no status", None, None, None, False)):
            with self.subTest(status=name), tempfile.TemporaryDirectory(prefix="qore-runner-helper-") as directory:
                fifo = self.start_helper(directory)
                # gdb's "info proc status": "process <pid>", then the lines of /proc/<pid>/status
                status = ""
                if uids is not None:
                    # keys with digits too, as the kernel prints them (x86_Thread_features:)
                    status = ("printf 'process %%s\\nName:\\tsh\\nUid:\\t%s\\nGid:\\t%s\\n"
                              "x86_Thread_features:\\tshstk\\n%s' \"$2\"\n"
                              % ("\\t".join(map(str, uids)), "\\t".join(map(str, gids)),
                                 "" if caps is None else "CapInh:\\t%s\\nCapPrm:\\t%s\\nCapEff:\\t%s\\nCapAmb:\\t%s\\n"
                                 % ((none,) + caps)))
                gdb = Path(directory) / "helper-bin" / "gdb"
                gdb.write_text('#!/bin/sh\nwhile [ "$1" != -p ]; do shift; done\n' + status
                               + 'echo "#0  0x0000 in read () from libc"\n')
                latch = Path(directory) / "latch"
                os.mkfifo(latch)
                target = start_in_session(["sh", "-c", 'exec 3<>"$1"; read x <&3', "sh", str(latch)],
                                          env=dict(os.environ, QORE_TEST_PROCESS_MARK="77.5"),
                                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                self.addCleanup(end_session, target)
                answer = self.ask(fifo, "%d 5 77.5 gdb {reply}" % target.pid, directory)
                if sent:
                    self.assertEqual("#0  0x0000 in read () from libc\nqore-stack-capture-status: 0\n", answer)
                else:
                    self.assertEqual("qore-stack-capture: refused: process %d was not a process of the runner's user "
                                     "and group without capabilities while it was stopped for the capture\n"
                                     "qore-stack-capture-status: 126\n" % target.pid, answer)

    def test_the_helper_sends_nothing_of_a_process_that_changed_during_the_capture(self):
        # The process executes another program during the attach (as one that executes a set-user-ID program would):
        # the helper's stand-in gdb releases it from its first latch, and returns once the new program has opened the
        # second one; the output, which would be the new program's, is not sent
        with tempfile.TemporaryDirectory(prefix="qore-runner-helper-") as directory:
            first = Path(directory) / "first"
            second = Path(directory) / "second"
            os.mkfifo(first)
            os.mkfifo(second)
            fifo = self.start_helper(directory)
            (Path(directory) / "helper-bin" / "gdb").write_text(
                '#!/bin/sh\necho released > "%s"\nexec 3> "%s"\necho "stacks of the other program"\n'
                % (first, second))
            target = start_in_session(
                ["sh", "-c", 'read x < "$1"; exec "$3" -c "$4" "$2"', "sh", str(first), str(second),
                 sys.executable, STAYS],
                env=dict(os.environ, QORE_TEST_PROCESS_MARK="77.9"), stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL)
            self.addCleanup(end_session, target)
            answer = self.ask(fifo, "%d 5 77.9 gdb {reply}" % target.pid, directory)
            self.assertEqual("qore-stack-capture: refused: process %d changed during the capture\n"
                             "qore-stack-capture-status: 126\n" % target.pid, answer)

    def test_the_helper_refuses_any_request_but_a_test_s_process(self):
        with tempfile.TemporaryDirectory(prefix="qore-runner-helper-") as directory:
            fifo = self.start_helper(directory)
            # a process of the user without a test's mark: this harness
            me = os.getpid()
            # a process that has ended (and been reaped), and a zombie, which are no restriction of ptrace
            ended = subprocess.Popen(["true"])
            ended.wait()
            zombie = subprocess.Popen(["true"])
            self.addCleanup(zombie.wait)
            os.waitid(os.P_PID, zombie.pid, os.WEXITED | os.WNOWAIT)
            cases = [
                ("%d 5 1.1 gdb {reply}" % me, "process %d is not a process of the test with mark 1.1" % me),
                ("%d 5 1.1 gdb {reply}" % ended.pid, "process %d has ended" % ended.pid),
                ("%d 5 1.1 gdb {reply}" % zombie.pid, "process %d has ended" % zombie.pid),
            ]
            # a process of another user, if there is one
            other = next((pid for pid in sorted(int(e) for e in os.listdir("/proc") if e.isdigit())
                          if self.owner(pid) not in (None, os.geteuid())), None)
            if other is not None:
                cases.append(("%d 5 1.1 gdb {reply}" % other,
                              "process %d is not a process of the runner's user" % other))
            for request, why in cases + [
                    ("x 5 1.1 gdb {reply}", "the process ID is not a number"),
                    ("01 5 1.1 gdb {reply}", "the process ID is not a number"),
                    ("1234567890 5 1.1 gdb {reply}", "the process ID is not a number"),
                    ("%d 0 1.1 gdb {reply}" % me, "the time limit is not 1 to 300 seconds"),
                    ("%d 301 1.1 gdb {reply}" % me, "the time limit is not 1 to 300 seconds"),
                    ("%d 5 a.1 gdb {reply}" % me, "the mark is not a test's"),
                    ("%d 5 1.1 sh {reply}" % me, "the tool is neither gdb nor eu-stack"),
                    ("%d 5 1.1 /bin/sh {reply}" % me, "the tool is neither gdb nor eu-stack")]:
                with self.subTest(request=request):
                    answer = self.ask(fifo, request, directory)
                    self.assertEqual("qore-stack-capture: refused: %s\nqore-stack-capture-status: 126\n" % why,
                                     answer)
            # a request it cannot answer: logged, and the helper goes on
            for request in ("%d 5 1.1 gdb relative/reply" % me, "%d 5 1.1 gdb /tmp/a;b" % me, "%d 5 1.1 gdb" % me):
                self.send(fifo, request)
            answer = self.ask(fifo, "%d 5 1.1 sh {reply}" % me, directory)
            self.assertIn("refused", answer)
            self.helper_log.seek(0)
            log = self.helper_log.read()
            self.assertIn("a request whose reply is not an absolute path is ignored", log)
            self.assertIn("a request whose reply has other characters is ignored", log)
            self.assertIn("a request that is not five fields is ignored", log)

    def test_a_helper_that_ptrace_restricts_is_reported_once(self):
        # A helper without CAP_SYS_PTRACE (a pod that is not privileged) may not read the environment of the test's
        # processes, and refuses each request with "ptrace restricted": the runner reports it once, as a refused
        # attach, and asks for no other process.  The helper is a stand-in that answers every request so.
        with tempfile.TemporaryDirectory(prefix="qore-runner-helper-") as directory:
            fifo = Path(directory) / "requests"
            os.mkfifo(fifo, 0o600)
            requests = Path(directory) / "requests.log"
            helper = start_in_session(["sh", "-c", """
                exec 3<> "$1"
                while read -r pid secs mark tool reply <&3; do
                    echo "$pid $tool" >> "$2"
                    printf 'qore-stack-capture: refused: ptrace restricted: the helper may not read %s\\n%s\\n' \
                        "the environment of process $pid (it has no CAP_SYS_PTRACE)" \
                        "qore-stack-capture-status: 126" > "$reply"
                done""", "sh", str(fifo), str(requests)], stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL)
            self.addCleanup(end_session, helper)
            output, pids, result, elapsed, root = self.run_test(
                children=2, scripts={"gdb": '#!/bin/sh\necho "the runner ran gdb $*"\n'},
                QORE_TEST_STACK_CAPTURE_HELPER=str(fifo), QORE_TEST_STACK_TOOLS="gdb eu-stack")
            pids.sort()
            lines = [line for line in output.splitlines() if line.startswith("*** ptrace restricted")]
            self.assertEqual(1, len(lines), output)
            self.assertIn("no stacks captured (processes %d %d)" % tuple(pids), lines[0])
            self.assertEqual(["%d gdb" % pids[0]], requests.read_text().splitlines())
            self.assertNotIn("the runner ran gdb", output)
            self.assert_before_signals(output)

    def test_a_helper_that_does_not_read_its_requests_is_not_waited_for(self):
        # a FIFO without a reader (a helper that has ended): the runner runs the tool itself
        with tempfile.TemporaryDirectory(prefix="qore-runner-helper-") as directory:
            fifo = Path(directory) / "requests"
            os.mkfifo(fifo, 0o600)
            output, pids, result, elapsed, root = self.run_test(
                scripts={"gdb": '#!/bin/sh\necho "the runner ran gdb $*"\n'},
                QORE_TEST_STACK_CAPTURE_HELPER=str(fifo))
            stacks = self.capture(output, pids[0])
            self.assertIn("the stack capture helper (%s) does not read its requests: gdb runs as the runner's user"
                          % fifo, stacks)
            self.assertIn("the runner ran gdb", stacks)
            self.assertLess(elapsed, LIMIT + 25, output)

    def test_a_zombie_is_reported_as_ended_and_not_as_a_refused_attach(self):
        # gdb 17 prints the same notice for a zombie as for a refused attach ("Could not attach to process", exit 0):
        # a child that the test has not reaped is reported as ended, and the next process is still captured
        with tempfile.TemporaryDirectory(prefix="qore-runner-stacks-") as directory:
            tree = RunnerTree(directory)
            latch = tree.root / "latch"
            os.mkfifo(latch)
            tree.add_test("zombie.qtest", f"""\
                # test-timeout: {LIMIT}
                import os
                import subprocess
                import sys
                zombie = subprocess.Popen(["true"])
                os.waitid(os.P_PID, zombie.pid, os.WEXITED | os.WNOWAIT)
                print("zombie %d" % zombie.pid, flush=True)
                child = subprocess.Popen([sys.executable, "-c",
                                          "import os, sys; os.read(os.open(sys.argv[1], os.O_RDWR), 1)",
                                          {str(latch)!r}])
                print("child %d" % child.pid, flush=True)
                child.wait()
                """)
            # A gdb that ends the process during its attach, and then prints gdb 17's notice, as for a refused attach.
            # It prints the notice only once the process has exited (its pidfd is readable): kill() only queues the
            # signal, and a stand-in that printed at once let the runner find the process still running (seen once in
            # about 35 runs on a loaded host; right after kill -KILL a blocked Python process was still running in
            # 1 of 200 trials), which it then took for a refused attach.
            tree.write_script("gdb", f"""\
                #!{sys.executable}
                import os
                import select
                import signal
                import sys
                args = sys.argv[1:]
                if "-p" not in args:
                    sys.exit(0)
                pid = int(args[args.index("-p") + 1])
                with open({str(tree.root / "calls")!r}, "a") as f:
                    f.write("attached to %d\\n" % pid)
                fd = os.pidfd_open(pid)
                os.kill(pid, signal.SIGKILL)
                select.select([fd], [], [])
                print({GDB17_REFUSED!r})
                """)
            result = tree.run(QORE_TEST_STACK_TOOLS="gdb")
            output = RunnerTree.sections(result.output)["zombie.qtest"]
            zombie = int(re.search(r"^zombie (\d+)$", output, re.M).group(1))
            child = int(re.search(r"^child (\d+)$", output, re.M).group(1))
            # the zombie is found before any attach; the process that ended during the attach after it
            self.assertIn("*** process %d has ended ***" % zombie, output)
            self.assertIn("*** process %d has ended ***" % child, output)
            self.assertNotIn("ptrace restricted", output)
            self.assertEqual(["attached to %d" % child], (tree.root / "calls").read_text().splitlines())

    @unittest.skipUnless(HAVE_CTYPES, "the child ends its main thread with pthread_exit() through ctypes")
    def test_a_process_whose_main_thread_has_ended_is_captured(self):
        # A process whose main thread has ended (pthread_exit()) while another thread runs shows that thread as a
        # zombie (state Z): it was reported as ended, without its stacks.  The child ends its main thread once a
        # process of its own, which the kernel sends SIGTERM when the thread that started it ends (PR_SET_PDEATHSIG),
        # is ready; that process then reports that the main thread has ended.
        child = textwrap.dedent("""\
            import ctypes
            import os
            import subprocess
            import sys
            import threading
            r, w = os.pipe()
            threading.Thread(target=os.read, args=(r, 1)).start()
            ready_r, ready_w = os.pipe()
            watcher = subprocess.Popen(
                [sys.executable, "-c", "import os, signal, sys\\n"
                 "signal.signal(signal.SIGTERM, lambda *a: print('main thread ended', flush=True))\\n"
                 "os.write(int(sys.argv[1]), b'x')\\n"
                 "r, w = os.pipe()\\n"
                 "os.read(r, 1)\\n", str(ready_w)],
                pass_fds=(ready_w,), preexec_fn=lambda: ctypes.CDLL(None).prctl(1, 15))
            os.close(ready_w)
            os.read(ready_r, 1)
            print("child %d started" % os.getpid(), flush=True)
            ctypes.CDLL(None).pthread_exit(None)
            """)
        with tempfile.TemporaryDirectory(prefix="qore-runner-stacks-") as directory:
            tree = RunnerTree(directory)
            (tree.tests / "tests").mkdir(parents=True)
            (tree.tests / "tests" / "threads_child.py").write_text(child)
            latch = tree.root / "latch"
            os.mkfifo(latch)
            tree.add_test("threads.qtest", f"""\
                # test-timeout: {LIMIT}
                import os
                import subprocess
                import sys
                subprocess.Popen([sys.executable, "-B", os.path.join(os.path.dirname(sys.argv[0]), "threads_child.py")])
                os.read(os.open({str(latch)!r}, os.O_RDWR), 1)
                """)
            tree.write_script("printing-tool", '#!/bin/sh\necho "stacks of $1 from a stand-in"\n')
            result = tree.run(QORE_TEST_STACK_TOOLS="printing-tool")
            output = RunnerTree.sections(result.output)["threads.qtest"]
            pid = int(re.search(r"^child (\d+) started$", output, re.M).group(1))
            self.assertIn("main thread ended", output)
            self.assertIn("stacks of %d from a stand-in" % pid, output)
            self.assertNotIn("*** process %d has ended ***" % pid, output)

    def test_a_scan_of_proc_that_blocks_is_bounded(self):
        # A read of /proc/<pid>/environ waits while the process holds the lock of its memory map (D state); the scan
        # for the test's mark is a stand-in awk that blocks on a latch, which the runner's limit ends
        real = shutil.which("awk")
        scripts = {
            "awk": '#!/bin/sh\nfor arg in "$@"; do case "$arg" in stack_mark=*) read line < "{latch}" ;; esac; done\n'
                   'exec \'%s\' "$@"\n' % real,
            "printing-tool": '#!/bin/sh\necho "stacks of $1 from a stand-in"\n',
        }
        output, pids, result, elapsed, root = self.run_test(scripts=scripts, QORE_TEST_STACK_TOOLS="printing-tool")
        self.assertIn("*** the scan of the processes of test", output)
        self.assertIn("with its mark (/proc/*/environ) did not end within 10s", output)
        # the child is still found as the test's descendant, and captured
        self.assertIn("stacks of %d from a stand-in" % pids[0], self.capture(output, pids[0]))
        self.assertLess(elapsed, LIMIT + 10 + 25, output)


if __name__ == "__main__":
    unittest.main()
