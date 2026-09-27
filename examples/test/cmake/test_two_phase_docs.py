#!/usr/bin/env python3
"""Exercise the exported documentation helper with CMAKE_EXECUTABLE (or cmake).

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


class TwoPhaseDocsTest(unittest.TestCase):
    def configure(self, root, enabled=True, missing_target=False, text="@literal@ ${literal}"):
        source = root / "source with spaces"
        build = root / "build~snapshot with spaces"
        source.mkdir(exist_ok=True)
        module_path = f"{source}/qlib:{build}/modules"
        (source / "check-env.py").write_text(f'''import os
from pathlib import Path
assert os.environ["QORE_MODULE_DIR"] == {module_path!r}
assert os.environ["QORE_DOC_DEFINES"] == "QORE_QDX_RUN,Unix"
Path({str(build / "qdx-environment-ok")!r}).touch()
''')
        (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(DocHelper NONE)
set(QORE_MODULE_DIR_FOR_DOCS "{module_path}")
set(QORE_DOC_DEFINES "QORE_QDX_RUN,Unix")
include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")
set(DOXYGEN_FOUND {"TRUE" if enabled else "FALSE"})
set(DOXYGEN_EXECUTABLE "${{CMAKE_COMMAND}}" -E echo)
set(QORE_QDX_COMMAND "{sys.executable}" "${{CMAKE_SOURCE_DIR}}/check-env.py")
add_custom_target(docs)
add_custom_target(docs-First COMMAND "${{CMAKE_COMMAND}}" -E touch first-built)
{"" if missing_target else 'add_custom_target(docs-Second COMMAND "${CMAKE_COMMAND}" -E touch second-built)'}
file(WRITE "${{CMAKE_BINARY_DIR}}/Doxyfile" [=[{text}
]=])
qore_binary_module_two_phase_docs(xml "First;Second")
''')
        result = subprocess.run([CMAKE, "-S", str(source), "-B", str(build)],
                                text=True, capture_output=True, timeout=30)
        if not missing_target:
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertNotIn("Warning", result.stderr)
        return build, result

    def test_literal_content_dependencies_and_reconfigure(self):
        with tempfile.TemporaryDirectory(prefix="qore-doc-helper-") as directory:
            root = Path(directory)
            build, _ = self.configure(root)
            for literal in ("@literal@ ${literal}", "changed @untouched@ ${untouched}"):
                self.configure(root, text=literal)
                final = (build / "Doxyfile.final").read_text()
                self.assertTrue(final.startswith(literal + "\n"), final)
                self.assertEqual(1, final.count("WARN_IF_DOC_ERROR = NO"))
                self.assertEqual(1, final.count("WARN_IF_DOC_ERROR = YES"))
                self.assertGreater(final.index("WARN_IF_DOC_ERROR = YES"),
                                   final.index("WARN_IF_DOC_ERROR = NO"))
                for name in ("First", "Second"):
                    self.assertIn(f"{build}/{name}.tag=../../{name}/html", final)
                result = subprocess.run([CMAKE, "--build", str(build), "--target", "docs", "-j2"],
                                        text=True, capture_output=True, timeout=30)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertNotIn("Warning", result.stderr)
                self.assertTrue((build / "first-built").is_file())
                self.assertTrue((build / "second-built").is_file())
                self.assertTrue((build / "qdx-environment-ok").is_file())

    def test_disabled_documentation(self):
        with tempfile.TemporaryDirectory(prefix="qore-doc-helper-") as directory:
            build, _ = self.configure(Path(directory), enabled=False)
            self.assertFalse((build / "Doxyfile.final").exists())
            self.assertNotIn("WARN_IF_DOC_ERROR", (build / "Doxyfile").read_text())

    def test_missing_dependency_is_an_error(self):
        with tempfile.TemporaryDirectory(prefix="qore-doc-helper-") as directory:
            _, result = self.configure(Path(directory), missing_target=True)
            self.assertNotEqual(0, result.returncode)
            self.assertIn('"docs-Second"', result.stderr)
            self.assertIn("does not exist", " ".join(result.stderr.split()))


if __name__ == "__main__":
    unittest.main()
