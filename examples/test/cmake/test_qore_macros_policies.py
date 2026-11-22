#!/usr/bin/env python3
"""Verify the policy scope of cmake/QoreMacros.cmake (qore#7189).

The file sets policy CMP0116 to NEW for the rules that its functions create, so that the Ninja generators give Ninja
the depfiles that qcc writes in a form it accepts (otherwise every qmod is compiled again by every Ninja build;
examples/test/cmake/test_aot_runtime_identity_rebuild.py checks the builds with Ninja when it is installed).  The
setting must stay in the file: including it changes no policy of the code that includes it, also in a script run with
cmake -P, where include() pushes no policy scope of its own (CMP0011 is not set), and gives no warning.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")


class QoreMacrosPoliciesTest(unittest.TestCase):
    def setUp(self):
        if not shutil.which(CMAKE):
            self.skipTest("CMake is required")
        directory = tempfile.TemporaryDirectory(prefix="qore-macros-policies-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)

    def run_script(self, text):
        script = self.root / "check.cmake"
        script.write_text(text)
        result = subprocess.run([CMAKE, "-P", str(script)], capture_output=True, text=True, timeout=60,
                                cwd=self.root)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertEqual("", result.stderr, "including the file gives no warning")
        return result.stdout

    def cmake_has_policy(self):
        out = self.run_script('if (POLICY CMP0116)\n    message(STATUS "yes")\nendif ()\n')
        return "yes" in out

    def test_script_mode(self):
        """A script without cmake_minimum_required(): CMP0011 is not set, so include() pushes no policy scope"""
        if not self.cmake_has_policy():
            self.skipTest("this CMake has no policy CMP0116")
        out = self.run_script(f'''cmake_policy(GET CMP0116 before)
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
cmake_policy(GET CMP0116 after)
message(STATUS "before=[${{before}}] after=[${{after}}]")
''')
        self.assertIn("before=[] after=[]", out)

    def test_project_setting_kept(self):
        """Code at the minimum version of the Qore build, where the policy is not set, keeps it unset"""
        if not self.cmake_has_policy():
            self.skipTest("this CMake has no policy CMP0116")
        out = self.run_script(f'''cmake_minimum_required(VERSION 3.14)
cmake_policy(GET CMP0116 before)
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
cmake_policy(GET CMP0116 after)
message(STATUS "before=[${{before}}] after=[${{after}}]")
''')
        self.assertIn("before=[] after=[]", out)


if __name__ == "__main__":
    unittest.main()
