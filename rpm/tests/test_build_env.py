# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Source maps must survive CMake flag parsing and reject ambiguous paths."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BuildFlagsTest(unittest.TestCase):
    def run_flags(self, directory, destination):
        script = '''. "$1"
qore_set_source_prefix_maps "$2"
status=$?
python3 -c 'import json, os; print(json.dumps([os.environ["CFLAGS"], os.environ["CXXFLAGS"]]))'
exit "$status"
'''
        return subprocess.run(["sh", "-c", script, "flags-test", str(ROOT / "build-env.sh"), destination],
                              cwd=directory, env={**os.environ, "CFLAGS": "-O2 -g", "CXXFLAGS": '-O2 -DNAME="two words"'},
                              capture_output=True, text=True, timeout=30)

    def test_quoted_paths_preserve_compiler_arguments(self):
        with tempfile.TemporaryDirectory(prefix="qore flags ' dollar$() ") as temporary:
            destination = "/usr/src/debug/package with ' quote"
            result = self.run_flags(temporary, destination)
            self.assertEqual(0, result.returncode, result.stderr)
            c, cxx = map(shlex.split, json.loads(result.stdout))
            expected = "-ffile-prefix-map=" + str(Path(temporary).resolve()) + "=" + destination
            self.assertEqual(["-O2", "-g", expected], c)
            self.assertEqual(["-O2", "-DNAME=two words", expected], cxx)

    def test_invalid_maps_leave_flags_unchanged(self):
        with tempfile.TemporaryDirectory() as temporary:
            for destination in ("", ".", "/usr/src/debug/", "/usr/src/debug/first\nsecond",
                                "/usr/src/debug/../escape", "/usr/src/debug/package/..",
                                "/usr/src/debug/package/./source", "/usr/src/debug/package//source"):
                with self.subTest(destination=destination):
                    result = self.run_flags(temporary, destination)
                    self.assertNotEqual(0, result.returncode)
                    self.assertEqual(["-O2 -g", '-O2 -DNAME="two words"'], json.loads(result.stdout))
            ambiguous = Path(temporary) / "equals=source"
            ambiguous.mkdir()
            result = self.run_flags(ambiguous, "/usr/src/debug/package")
            self.assertNotEqual(0, result.returncode)
            self.assertIn("equals sign", result.stderr)


if __name__ == "__main__":
    unittest.main()
