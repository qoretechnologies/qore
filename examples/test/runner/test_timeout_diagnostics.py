#!/usr/bin/env python3
"""Exercise test-runner exit classification without waiting for a real timeout.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]


class TimeoutDiagnosticsTest(unittest.TestCase):
    def run_runner(self, exit_code, *, gnu=False, core_name="core.fixture",
                   kernel_pattern=None, fresh_core=True):
        with tempfile.TemporaryDirectory(prefix="qore-runner-diagnostics-") as directory:
            root = Path(directory)
            binaries = root / "bin"
            binaries.mkdir()
            tests = root / "examples/test"
            tests.mkdir(parents=True)
            (tests / "one.qtest").touch()
            (tests / "two.qtest").touch()
            (binaries / "libqore.so").touch()
            scripts = {
                "qore": '#!/bin/sh\ncase "$*" in *one.qtest*) '
                        'if [ "$FRESH_CORE" = 1 ]; then touch -t 203001010000 "$TEST_CORE_TARGET"; fi; '
                        'exit "$TEST_EXIT";; esac\nexit 0\n',
                "timeout": '#!/bin/sh\nif [ "$1" = --version ]; then '
                           '[ "$GNU_TIMEOUT" = 1 ] && echo GNU; exit 0; fi\n'
                           'if [ "$GNU_TIMEOUT" = 1 ]; then shift 4; fi\nshift\nexec "$@"\n',
                "gdb": '#!/bin/sh\necho "$*" >> "$GDB_MARKER"\necho captured-threads\n',
                "cat": '#!/bin/sh\nif [ "$1" = /proc/sys/kernel/core_pattern ]; then '
                       'printf "%s\\n" "$TEST_KERNEL_CORE_PATTERN"; else exec /bin/cat "$@"; fi\n',
                "sysctl": '#!/bin/sh\nexit 1\n',
            }
            for name, source in scripts.items():
                path = binaries / name
                path.write_text(source)
                path.chmod(0o755)
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
            }
            result = subprocess.run(["sh", str(ROOT / "run_tests.sh")], cwd=root,
                                    env=env, capture_output=True, text=True, timeout=15)
            if marker.exists():
                self.assertIn(str(target), marker.read_text())
            self.assertFalse(list(cores.glob("test-start.*")), "runner must remove its timestamp marker")
            return result, marker.exists()

    def test_timeouts_remain_failures_without_debugger_rerun(self):
        for code in (124, 143):
            with self.subTest(code=code):
                result, debugger = self.run_runner(code)
                self.assertEqual(1, result.returncode, result.stdout + result.stderr)
                self.assertIn("TIMEOUT: test exceeded", result.stdout)
                self.assertNotIn("CRASH:", result.stdout)
                self.assertFalse(debugger, result.stdout)
                self.assertIn("Running test (2/2)", result.stdout)

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
        result, debugger = self.run_runner(124, gnu=True)
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertTrue(debugger)
        self.assertIn("captured-threads", result.stdout)
        self.assertNotIn("re-running", result.stdout)

    def test_obs_numeric_core_name_is_found_in_kernel_directory(self):
        result, debugger = self.run_runner(124, gnu=True, kernel_pattern="%p", core_name="4321")
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertTrue(debugger)
        self.assertIn("captured-threads", result.stdout)

    def test_stale_cores_are_not_attributed_to_a_timed_out_test(self):
        for pattern in (None, "%p"):
            with self.subTest(pattern=pattern):
                result, debugger = self.run_runner(124, gnu=True, kernel_pattern=pattern, fresh_core=False)
                self.assertEqual(1, result.returncode, result.stdout + result.stderr)
                self.assertFalse(debugger)

    def test_kernel_pattern_literals_and_spaces_are_preserved(self):
        result, debugger = self.run_runner(124, gnu=True, kernel_pattern="dump %%p [%e] %p",
                                           core_name="dump %p [qore] 4321")
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertTrue(debugger)

    def test_piped_core_handler_is_not_a_filesystem_path(self):
        result, debugger = self.run_runner(124, gnu=True, kernel_pattern="|/usr/bin/core-handler %p")
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertFalse(debugger)

    def test_success(self):
        result, debugger = self.run_runner(0)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertFalse(debugger, result.stdout)


if __name__ == "__main__":
    unittest.main()
