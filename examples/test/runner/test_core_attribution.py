#!/usr/bin/env python3
"""run_tests.sh attributes a core to a test only when it is the core of a process of that test, and bounds the
debugger that reads it (qore#6893).

run_tests.sh runs a tree of two tests, one.qtest and two.qtest (which passes), with stand-ins for qore, timeout, gdb, cat and sysctl.  The qore
stand-in ends as a test does that the runner's time limit ended (124, as GNU timeout reports it, in a run with a plain
time limit, -t) or that crashed (139), after writing the cores that the case needs to the runner's core directory: its own core, which holds the
test's mark (QORE_TEST_PROCESS_MARK, as a real core holds the environment of its process), and a core of another
program, written during the test, which does not.  The gdb stand-in records the core it is given; in the case of the
bound, it blocks on a FIFO that nothing writes, so the run ends only if the runner ends the debugger.  Nothing waits
for a time: the bound of the debugger is set to 1 s for that case only, and the case's own limit is a hang guard.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]

# the hang guard of a run of the runner
RUN_LIMIT = 120


# run_tests.sh reads the kernel's core pattern from /proc/sys/kernel/core_pattern, which the cat stand-in reports,
# and looks for the processes a test left running in /proc
@unittest.skipUnless(sys.platform.startswith("linux"), "the runner's core and process checks are Linux's")
class CoreAttributionTest(unittest.TestCase):
    def run_runner(self, exit_code, *, own_core, foreign_core, block_gdb=False, jobs=1):
        # a timed-out test is one that the plain timeout command (run_tests.sh -t) ended (exit status 124): only then,
        # or when the watchdog's deadline expired, does the runner read the core of a test that timed out (#6876)
        plain_timeout = exit_code == 124
        with tempfile.TemporaryDirectory(prefix="qore-runner-core-attribution-") as directory:
            root = Path(directory)
            binaries = root / "bin"
            binaries.mkdir()
            tests = root / "examples/test"
            tests.mkdir(parents=True)
            # the time limit of the test, which bounds a debugger that runs it again together with the debugger's bound
            (tests / "one.qtest").write_text("# test-timeout: 1\n")
            # a second test, which passes, so that a run of two jobs runs tests in parallel
            (tests / "two.qtest").touch()
            # run_tests.sh reads its helpers from the tree it runs in
            helpers = tests / "docker_test"
            helpers.mkdir()
            (helpers / "ci-job-processes.sh").symlink_to(ROOT / "examples/test/docker_test/ci-job-processes.sh")
            (binaries / "libqore.so").touch()
            cores = root / "cores"
            cores.mkdir()
            latch = root / "never"
            os.mkfifo(latch)
            marker = root / "gdb-called"
            scripts = {
                # writes the foreign core first, then its own, which is the newest: a runner that takes the first
                # or the newest new core is caught either way by the core it names.  Only the run of one.qtest does:
                # run_tests.sh also runs qore before the tests (qore --version, its build-match check), and cores
                # written then exist before the test starts, so the test's own core would rewrite a core that the
                # runner had already seen, which BusyBox find -newer (whole seconds, Alpine) does not report as new
                "qore": '#!/bin/sh\ncase "$*" in *one.qtest*) ;; *) exit 0 ;; esac\n'
                        'if [ "$FOREIGN_CORE" = 1 ]; then printf "OTHER_PROGRAM=1\\0" > "$CORE_DIR/core.other.1"; fi\n'
                        'if [ "$OWN_CORE" = 1 ]; then '
                        'printf "QORE_TEST_PROCESS_MARK=%s\\0" "$QORE_TEST_PROCESS_MARK" > "$CORE_DIR/core.own"; fi\n'
                        'exit "$TEST_EXIT"\n',
                # GNU timeout: runs its command without the limit the runner gives it, except for the debugger,
                # whose bound is the case's (the limit is the first argument after the options)
                "timeout": '#!/bin/sh\nif [ "$1" = --version ]; then echo "timeout (GNU coreutils)"; exit 0; fi\n'
                           'while [ $# -gt 0 ]; do case "$1" in -s|-k) shift 2 ;; -*) shift ;; *) break ;; esac; '
                           'done\nlimit=$1; shift\n'
                           'case "$1" in gdb|lldb) exec /usr/bin/timeout "$limit" "$@" ;; esac\nexec "$@"\n',
                "gdb": '#!/bin/sh\nfor a in "$@"; do last=$a; done\necho "$last" >> "$GDB_MARKER"\n'
                       'if [ "$BLOCK_GDB" = 1 ]; then read -r line < "$GDB_LATCH"; fi\necho captured-threads\n',
                "cat": '#!/bin/sh\nif [ "$1" = /proc/sys/kernel/core_pattern ]; then '
                       'printf "%s\\n" "$TEST_KERNEL_CORE_PATTERN"; else exec /bin/cat "$@"; fi\n',
                "sysctl": '#!/bin/sh\nexit 1\n',
            }
            for name, source in scripts.items():
                path = binaries / name
                path.write_text(source)
                path.chmod(0o755)
            env = {
                # CI: the runner does not set the machine's core pattern, which a root run outside CI would do
                "CI": "1",
                "PATH": str(binaries) + os.pathsep + os.defpath,
                "QORE_BINARY": str(binaries / "qore"),
                "LIBQORE_BINARY": str(binaries / "libqore.so"),
                "CORE_DIR": str(cores),
                "QORE_TEST_JOBS": str(jobs),
                "TEST_EXIT": str(exit_code),
                "GDB_MARKER": str(marker),
                "GDB_LATCH": str(latch),
                "BLOCK_GDB": str(int(block_gdb)),
                "OWN_CORE": str(int(own_core)),
                "FOREIGN_CORE": str(int(foreign_core)),
                "TEST_KERNEL_CORE_PATTERN": "core",
                # the stand-in qore is built from no commit, so run_tests.sh's build-match check would refuse it
                # before any test runs; this test is about core attribution, not about which build runs
                "QORE_TEST_ALLOW_BUILD_MISMATCH": "1",
            }
            if block_gdb:
                env["QORE_TEST_DEBUGGER_TIMEOUT"] = "1"
            args = ["sh", str(ROOT / "run_tests.sh")]
            if plain_timeout:
                args.append("-t")
            result = subprocess.run(args, cwd=root, env=env, capture_output=True, text=True, timeout=RUN_LIMIT)
            debugged = marker.read_text().split() if marker.exists() else []
            return result, [Path(c).name for c in debugged]

    def test_a_foreign_core_is_not_the_timed_out_tests(self):
        for jobs in (1, 2):
            with self.subTest(jobs=jobs):
                result, debugged = self.run_runner(124, own_core=False, foreign_core=True, jobs=jobs)
                self.assertEqual(1, result.returncode, result.stdout + result.stderr)
                self.assertIn("timeout command returned status 124", result.stdout)
                self.assertEqual([], debugged, result.stdout)

    def test_a_foreign_core_is_not_the_crashed_tests(self):
        result, debugged = self.run_runner(139, own_core=False, foreign_core=True)
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertIn("CRASH: test killed by signal 11", result.stdout)
        self.assertNotIn("core.other.1", debugged, result.stdout)

    def test_the_tests_core_is_read_beside_a_foreign_core(self):
        for code in (124, 139):
            for jobs in (1, 2):
                with self.subTest(code=code, jobs=jobs):
                    result, debugged = self.run_runner(code, own_core=True, foreign_core=True, jobs=jobs)
                    self.assertEqual(1, result.returncode, result.stdout + result.stderr)
                    self.assertEqual(["core.own"], debugged, result.stdout)
                    self.assertIn("captured-threads", result.stdout)

    def test_a_blocked_debugger_is_ended(self):
        for code in (124, 139):
            with self.subTest(code=code):
                result, debugged = self.run_runner(code, own_core=True, foreign_core=False, block_gdb=True)
                self.assertEqual(1, result.returncode, result.stdout + result.stderr)
                self.assertEqual(["core.own"], debugged, result.stdout)
                self.assertNotIn("captured-threads", result.stdout)
                self.assertIn("did not end within 1 s", result.stdout)

    def test_a_blocked_debugger_that_runs_the_test_again_is_ended(self):
        # a crash without a core: the test runs again under the debugger, bounded by its own time limit (1 s) and
        # the debugger's bound (1 s)
        result, debugged = self.run_runner(139, own_core=False, foreign_core=False, block_gdb=True)
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertIn("re-running under gdb", result.stdout)
        self.assertEqual(1, len(debugged), result.stdout)
        self.assertNotIn("captured-threads", result.stdout)
        self.assertIn("did not end within 2 s", result.stdout)


if __name__ == "__main__":
    unittest.main()
