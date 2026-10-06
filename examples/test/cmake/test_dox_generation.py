#!/usr/bin/env python3
"""Exercise documentation output publication and generator dependencies.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")


class DoxGenerationTest(unittest.TestCase):
    def command(self, args, success=True):
        result = subprocess.run(args, capture_output=True, text=True, timeout=30)
        if success:
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertNotIn("warning", (result.stdout + result.stderr).lower())
        else:
            self.assertNotEqual(0, result.returncode)
        return result

    def test_failed_conversion_never_publishes_partial_output(self):
        for previous_output in (False, True):
            with self.subTest(previous_output=previous_output), tempfile.TemporaryDirectory(
                    prefix="qore doc generation ") as directory:
                root = Path(directory)
                source = root / "source"
                source.mkdir()
                build = root / "build"
                generator = source / "qpp-fixture"
                generator.write_text(f'''#!{sys.executable}
from pathlib import Path
import sys
root = Path(__file__).parent
output = Path(next(a.split("=", 1)[1] for a in sys.argv if a.startswith("--output=")))
assert "--table-strict" in sys.argv
with (root / "calls").open("a") as log:
    log.write("called\\n")
if (root / "fail").exists():
    output.write_text("partial")
    sys.exit(1)
output.write_text((root / "page.dox.tmpl").read_text())
''')
                generator.chmod(0o755)
                template = source / "page.dox.tmpl"
                template.write_text("complete documentation\n")
                (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(DocGeneration NONE)
include("{ROOT}/cmake/QoreMacros.cmake")
set(QORE_QPP_EXECUTABLE "${{CMAKE_SOURCE_DIR}}/qpp-fixture")
set(QORE_DOX_TABLE_STRICT ON)
qore_wrap_dox(pages page.dox.tmpl)
add_custom_target(docs DEPENDS ${{pages}})
''')
                self.command([CMAKE, "-S", str(source), "-B", str(build)])
                command = [CMAKE, "--build", str(build), "--target", "docs"]
                output = build / "page.dox"
                if previous_output:
                    self.command(command)
                    self.assertEqual(template.read_text(), output.read_text())
                    # No sleeps: put the old output before both prerequisites.
                    os.utime(output, (1, 1))
                (source / "fail").touch()
                self.command(command, success=False)
                if previous_output:
                    self.assertEqual("complete documentation\n", output.read_text())
                else:
                    self.assertFalse(output.exists())
                (source / "fail").unlink()
                self.command(command)
                self.assertEqual(template.read_text(), output.read_text())
                self.assertFalse((build / "page.dox.tmp").exists())
                calls = (source / "calls").read_text()
                self.command(command)
                self.assertEqual(calls, (source / "calls").read_text())
                # Only qpp is newer than the output; a generator update must rebuild.
                os.utime(template, (1, 1))
                os.utime(output, (2, 2))
                self.command(command)
                self.assertEqual(calls + "called\n", (source / "calls").read_text())


if __name__ == "__main__":
    unittest.main()
