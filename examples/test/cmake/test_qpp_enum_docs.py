#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Generate enum documentation with QPP and resolve its values with Doxygen."""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
QPP = Path(os.environ.get("QORE_QPP_EXECUTABLE", ROOT / "build-debug/qpp")).resolve()
WRAPPER = shlex.split(os.environ.get("QPP_TEST_WRAPPER", ""))


class EnumDocumentationTest(unittest.TestCase):
    def invoke(self, command, **kwargs):
        result = subprocess.run(command, capture_output=True, text=True, timeout=90, **kwargs)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        return result

    def test_documented_and_undocumented_values_have_resolvable_anchors(self):
        with tempfile.TemporaryDirectory(prefix="qore-enum-doc-") as directory:
            root = Path(directory)
            source = root / "Enums.qpp"
            source.write_text('''//! Connection policy
/** Values control a connection policy. */
enum Qore::DocTest::Policy : int {
    //! Reject unknown keys
    Reject = 0,
    //! Trust on first use: café
    Trust,
    Undocumented = 5,
};
//! String mode
/** Values select an output mode. */
enum Qore::DocTest::Mode : string {
    //! Compact output
    Compact = "compact",
};
''')
            header = root / "Enums.dox.h"
            self.invoke([*WRAPPER, str(QPP), f"--output={root}/Enums.cpp",
                         f"--dox-output={header}", str(source)])
            text = header.read_text()
            self.assertIn("//! Reject unknown keys\n    Reject = 0,", text)
            self.assertIn("//! Trust on first use: café\n    Trust,", text)
            self.assertIn("    Undocumented = 5,", text)
            (root / "references.dox").write_text('''/** @mainpage Enum value references
@ref Qore::DocTest::Policy::Reject
@ref Qore::DocTest::Policy::Trust
@ref Qore::DocTest::Mode::Compact
*/
''')
            (root / "Doxyfile").write_text(f'''PROJECT_NAME = EnumDocs
INPUT = "{header}" "{root}/references.dox"
OUTPUT_DIRECTORY = "{root}/docs"
GENERATE_TAGFILE = "{root}/enums.tag"
GENERATE_HTML = YES
GENERATE_LATEX = NO
EXTRACT_ALL = YES
QUIET = YES
WARN_AS_ERROR = FAIL_ON_WARNINGS
''')
            result = self.invoke(["doxygen", str(root / "Doxyfile")], cwd=root)
            self.assertEqual("", result.stderr)
            values = ET.parse(root / "enums.tag").findall(".//member[@kind='enumvalue']")
            self.assertEqual({"Reject", "Trust", "Undocumented", "Compact"},
                             {value.findtext("name") for value in values}, (root / "enums.tag").read_text())
            self.assertTrue(all(value.findtext("anchor") for value in values))

    def test_invalid_enums_are_rejected_before_creating_outputs(self):
        with tempfile.TemporaryDirectory(prefix="qore-enum-invalid-") as directory:
            root = Path(directory)
            source = root / "Invalid.qpp"
            for declaration, diagnostic in (
                ("enum Qore::Broken : int {\n//! Missing value\n", "premature EOF reading enum"),
                ("enum Qore::Broken : bool {\n};\n", "invalid enum base type"),
                ("enum Qore::Broken : int\n", "missing '{'"),
                ("enum {\n};\n", "missing enum name"),
            ):
                with self.subTest(declaration=declaration):
                    source.write_text("//! Invalid enum\n/** Invalid input. */\n" + declaration)
                    result = subprocess.run([*WRAPPER, str(QPP), f"--output={root}/Invalid.cpp",
                        f"--dox-output={root}/Invalid.dox.h", str(source)], capture_output=True, text=True, timeout=90)
                    self.assertEqual(1, result.returncode, result.stdout + result.stderr)
                    self.assertIn(diagnostic, result.stderr)
                    self.assertFalse((root / "Invalid.cpp").exists())
                    self.assertFalse((root / "Invalid.dox.h").exists())


if __name__ == "__main__":
    unittest.main()
