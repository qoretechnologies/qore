#!/usr/bin/env python3
"""Exercise test-runner exit classification without waiting for a real timeout.

run_tests.sh runs a tree of two tests, one.qtest and two.qtest, with stand-ins for qore, timeout, gdb, cat and
sysctl. The first test exits with the status under test or receives SIGTERM after it reports its PID. The second
test passes. The timeout stand-in runs its command without a limit, so any timeout-like status comes from the child.
run_tests.sh exits with the number of failed tests.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_watchdog import STAND_IN_VERSION, stand_in_build  # noqa: E402


# run_tests.sh reads the kernel's core pattern from /proc/sys/kernel/core_pattern, which the cat stand-in reports,
# and looks for the processes a test left running in /proc
@unittest.skipUnless(sys.platform.startswith("linux"), "the runner's core and process checks are Linux's")
class TimeoutDiagnosticsTest(unittest.TestCase):
    def run_runner(self, exit_code, *, gnu=False, plain_timeout=False, core_name="core.fixture",
                   kernel_pattern=None, fresh_core=True, time_program=True):
        with tempfile.TemporaryDirectory(prefix="qore-runner-diagnostics-") as directory:
            root = Path(directory)
            binaries = root / "bin"
            binaries.mkdir()
            tests = root / "examples/test"
            tests.mkdir(parents=True)
            (tests / "one.qtest").touch()
            (tests / "two.qtest").touch()
            # run_tests.sh reads its helpers from the tree it runs in
            helpers = tests / "docker_test"
            helpers.mkdir()
            (helpers / "ci-job-processes.sh").symlink_to(ROOT / "examples/test/docker_test/ci-job-processes.sh")
            (binaries / "libqore.so").touch()
            # the test's core holds the test's mark, as a real core holds the environment of its process: a core that
            # does not is not the test's (qore#6893); touch -t reads its time in UTC (TZ=UTC0): a local time can name
            # two instants or none
            scripts = {
                "qore": '#!/bin/sh\n' + STAND_IN_VERSION + 'case "$*" in *one.qtest*) '
                        'if [ "$FRESH_CORE" = 1 ]; then '
                        'printf "QORE_TEST_PROCESS_MARK=%s\\0" "$QORE_TEST_PROCESS_MARK" > "$TEST_CORE_TARGET"; '
                        'TZ=UTC0 touch -t 203001010000 "$TEST_CORE_TARGET"; fi; '
                        'if [ "$TEST_EXIT" = TERM ]; then trap "exit 143" TERM; '
                        'printf "%s\\n" "$$" | (read -r pid && kill -TERM "$pid"); fi; '
                        'exit "$TEST_EXIT";; esac\nexit 0\n',
                # GNU timeout with GNU_TIMEOUT=1, else BusyBox's: runs its command without a limit, whether the
                # runner passes signal options ("-s ABRT -k 60 LIMIT cmd") or only a limit ("LIMIT cmd")
                "timeout": '#!/bin/sh\nif [ "$1" = --version ]; then '
                           '[ "$GNU_TIMEOUT" = 1 ] && echo "timeout (GNU coreutils)"; exit 0; fi\n'
                           'while [ $# -gt 0 ]; do case "$1" in -s|-k) shift 2 ;; -*) shift ;; *) break ;; esac; '
                           'done\nshift\nexec "$@"\n',
                "gdb": '#!/bin/sh\necho "$*" >> "$GDB_MARKER"\necho captured-threads\n',
                "cat": '#!/bin/sh\nif [ "$1" = /proc/sys/kernel/core_pattern ]; then '
                       'printf "%s\\n" "$TEST_KERNEL_CORE_PATTERN"; else exec /bin/cat "$@"; fi\n',
                "sysctl": '#!/bin/sh\nexit 1\n',
            }
            # -t (plain_timeout) times the tests: with time_program, a stand-in for GNU time ("time -f FORMAT cmd"),
            # else none, so that the runner falls back to the shell's time keyword or to no timing; either way the
            # host's time program is not used (QORE_TEST_TIME_BINS)
            if time_program:
                scripts["time"] = '#!/bin/sh\nif [ "$1" = -f ]; then shift 2; fi\nexec "$@"\n'
            for name, source in scripts.items():
                path = binaries / name
                path.write_text(source)
                path.chmod(0o755)
            # the tree is an archive of the stand-in build's commit (#6897)
            stand_in_build(root, binaries)
            cores = root / "cores"
            cores.mkdir()
            system_cores = root / "system cores"
            system_cores.mkdir()
            target = (system_cores if kernel_pattern is not None else cores) / core_name
            target.touch()
            os.utime(target, (1, 1))
            pattern = (str(system_cores / kernel_pattern) if kernel_pattern is not None
                       and not kernel_pattern.startswith("|") else kernel_pattern or "core")
            marker = root / "gdb-called"
            env = {
                # CI: the runner does not set the machine's core pattern, which a root run outside CI would do
                "CI": "1",
                "PATH": str(binaries) + os.pathsep + os.defpath,
                "QORE_BINARY": str(binaries / "qore"),
                "LIBQORE_BINARY": str(binaries / "libqore.so"),
                "CORE_DIR": str(cores),
                "TEST_EXIT": str(exit_code),
                "GDB_MARKER": str(marker),
                "GNU_TIMEOUT": str(int(gnu)),
                "FRESH_CORE": str(int(fresh_core)),
                "TEST_CORE_TARGET": str(target),
                "TEST_KERNEL_CORE_PATTERN": pattern,
                "QORE_TEST_TIME_BINS": str(binaries / "time") if time_program else "",
            }
            args = ["sh", str(ROOT / "run_tests.sh")]
            if plain_timeout:
                args.append("-t")
            result = subprocess.run(args, cwd=root, env=env, capture_output=True, text=True, timeout=120)
            if marker.exists():
                self.assertIn(str(target), marker.read_text())
            self.assertFalse(list(cores.glob("test-start.*")), "runner must remove its timestamp marker")
            return result, marker.exists()

    def test_child_status_cannot_report_a_watchdog_timeout(self):
        for code, trigger in ((124, 124), (143, 143), (143, "TERM")):
            with self.subTest(trigger=trigger):
                result, debugger = self.run_runner(trigger, gnu=True)
                self.assertEqual(1, result.returncode, result.stdout + result.stderr)
                self.assertIn(f"TEST EXIT: status {code}", result.stdout)
                self.assertNotIn("TIMEOUT:", result.stdout)
                self.assertNotIn("CRASH:", result.stdout)
                self.assertFalse(debugger, result.stdout)
                self.assertIn("Running test (2/2)", result.stdout)

    def test_plain_timeout_status_remains_ambiguous(self):
        for code in (124, 143):
            with self.subTest(code=code):
                result, debugger = self.run_runner(code, plain_timeout=True)
                self.assertEqual(1, result.returncode, result.stdout + result.stderr)
                self.assertIn(f"timeout command returned status {code}", result.stdout)
                self.assertNotIn("test exceeded", result.stdout)
                self.assertFalse(debugger, result.stdout)

    def test_unexpected_signal_keeps_crash_diagnostics(self):
        result, debugger = self.run_runner(139)
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertIn("CRASH: test killed by signal 11", result.stdout)
        self.assertNotIn("TIMEOUT:", result.stdout)
        self.assertTrue(debugger, result.stdout)

    def test_ordinary_failure_does_not_invoke_debugger(self):
        result, debugger = self.run_runner(1)
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertNotIn("TIMEOUT:", result.stdout)
        self.assertNotIn("CRASH:", result.stdout)
        self.assertFalse(debugger, result.stdout)

    def test_gnu_timeout_reads_fresh_local_core_without_rerunning_test(self):
        result, debugger = self.run_runner(124, gnu=True, plain_timeout=True)
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertTrue(debugger)
        self.assertIn("captured-threads", result.stdout)
        self.assertNotIn("re-running", result.stdout)

    def test_obs_numeric_core_name_is_found_in_kernel_directory(self):
        result, debugger = self.run_runner(124, gnu=True, plain_timeout=True, kernel_pattern="%p", core_name="4321")
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertTrue(debugger)
        self.assertIn("captured-threads", result.stdout)

    def test_stale_cores_are_not_attributed_to_a_timed_out_test(self):
        for pattern in (None, "%p"):
            with self.subTest(pattern=pattern):
                result, debugger = self.run_runner(124, gnu=True, plain_timeout=True,
                                                   kernel_pattern=pattern, fresh_core=False)
                self.assertEqual(1, result.returncode, result.stdout + result.stderr)
                self.assertFalse(debugger)

    def test_kernel_pattern_literals_and_spaces_are_preserved(self):
        result, debugger = self.run_runner(124, gnu=True, plain_timeout=True, kernel_pattern="dump %%p [%e] %p",
                                           core_name="dump %p [qore] 4321")
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertTrue(debugger)

    def test_piped_core_handler_is_not_a_filesystem_path(self):
        result, debugger = self.run_runner(124, gnu=True, plain_timeout=True, kernel_pattern="|/usr/bin/core-handler %p")
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertFalse(debugger)

    def test_timed_run_without_a_time_program(self):
        # -t with a timeout command but no time program: the runner must not hand the shell's time keyword to the
        # timeout command (which can only run programs: "exec: time: not found" made every test fail)
        for gnu in (False, True):
            with self.subTest(gnu=gnu):
                result, debugger = self.run_runner(0, gnu=gnu, plain_timeout=True, time_program=False)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertNotIn("not found", result.stderr)
                self.assertFalse(debugger)
                result, debugger = self.run_runner(1, gnu=gnu, plain_timeout=True, time_program=False)
                self.assertEqual(1, result.returncode, result.stdout + result.stderr)
                self.assertNotIn("not found", result.stderr)

    def test_timed_run_with_a_time_program(self):
        # -t with a time program and a timeout command: the time program runs under the timeout command
        result, debugger = self.run_runner(0, gnu=True, plain_timeout=True, time_program=True)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertNotIn("not found", result.stderr)

    def test_success(self):
        result, debugger = self.run_runner(0)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertFalse(debugger, result.stdout)


if __name__ == "__main__":
    unittest.main()
