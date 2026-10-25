#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""QPP writes each progress message whole, prefix included, to standard output."""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
QPP = Path(os.environ.get("QORE_QPP_EXECUTABLE", ROOT / "build/qpp")).resolve()
WRAPPER = shlex.split(os.environ.get("QPP_TEST_WRAPPER", ""))


class QppLogOutputTest(unittest.TestCase):
    def setUp(self):
        if not QPP.is_file():
            raise unittest.SkipTest("Build qpp or set QORE_QPP_EXECUTABLE to run the QPP integration tests")

    def run_qpp(self, *args):
        result = subprocess.run([*WRAPPER, str(QPP), *args], capture_output=True, text=True, timeout=90)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        return result

    def test_progress_messages_are_whole_lines_on_stdout(self):
        with tempfile.TemporaryDirectory(prefix="qore-qpp-log-") as directory:
            root = Path(directory)
            source = root / "Empty.qpp"
            source.write_text("/** @file Empty.qpp defines nothing */\n")
            cpp = root / "Empty.cpp"
            dox = root / "Empty.dox.h"
            result = self.run_qpp(f"--output={cpp}", f"--dox-output={dox}", str(source))
            # a prefix written to stderr and its message to stdout leave "qpp: qpp: " on stderr, with no newline,
            # and messages without a prefix on stdout
            self.assertNotIn("qpp:", result.stderr)
            if not WRAPPER:
                self.assertEqual("", result.stderr)
            self.assertEqual([f"qpp: creating cpp file {source} -> {cpp}",
                              f"qpp: creating dox file {source} -> {dox}"],
                             result.stdout.splitlines())

    def test_debug_messages_are_whole_lines_on_stdout(self):
        with tempfile.TemporaryDirectory(prefix="qore-qpp-log-debug-") as directory:
            root = Path(directory)
            source = root / "Function.qpp"
            source.write_text("/** @file Function.qpp defines one function */\n"
                              "//! Returns one\n"
                              "/** @return one\n"
                              "*/\n"
                              "int qpp_log_test() [flags=CONSTANT] {\n"
                              "    return 1;\n"
                              "}\n")
            result = self.run_qpp("--verbose=4", f"--output={root}/Function.cpp",
                                  f"--dox-output={root}/Function.dox.h", str(source))
            self.assertNotIn("qpp:", result.stderr)
            lines = result.stdout.splitlines()
            self.assertTrue(any(line.startswith("qpp: DEBUG: ") for line in lines), result.stdout)
            self.assertEqual([], [line for line in lines if not line.startswith("qpp: ")], result.stdout)


if __name__ == "__main__":
    unittest.main()
