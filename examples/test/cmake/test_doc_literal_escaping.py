# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Check the displayed text of punctuation-heavy documentation examples."""
from html.parser import HTMLParser
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


REPOSITORY = Path(__file__).resolve().parents[3]


class VisibleText(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.text = []

    def handle_data(self, data):
        self.text.append(data)


class DocumentationLiteralTest(unittest.TestCase):
    def test_source_literals_render_as_documented_characters(self):
        http = (REPOSITORY / "lib/QC_HTTPClient.qpp").read_text()
        notes = (REPOSITORY / "doxygen/lang/900_release_notes.dox.tmpl").read_text()
        patterns = [
            (http, r"percent encoding are:\n\s*(@verbatim[\s\S]*?@endverbatim)", "{}|\\^~[]`"),
            (notes, r"(<tt>esmtptls:[^\n]+?</tt>)", "esmtptls://user:password@smtp.example.com"),
            (notes, r"can be (<tt>[^\n]+?</tt>, <tt>[^\n]+?</tt>, or <tt>[^\n]+?</tt>)",
             "\\n, \\r, or \\r\\n"),
            (notes, r"scanner fix: accept (<tt>[^\n]+?</tt>) as whitespace", "\\r"),
        ]
        snippets = []
        for source, pattern, expected in patterns:
            matches = re.findall(pattern, source)
            self.assertEqual(1, len(matches), pattern)
            snippets.append(matches[0])
        with tempfile.TemporaryDirectory(prefix="qore-doc-literal-") as name:
            root = Path(name)
            (root / "input.dox").write_text(
                "/** @page literal Literal examples\n\n" + "\n\n".join(snippets) + "\n*/\n")
            (root / "Doxyfile").write_text(
                "INPUT = input.dox\nOUTPUT_DIRECTORY = output\nGENERATE_LATEX = NO\n"
                "QUIET = YES\nWARN_AS_ERROR = YES\nMARKDOWN_SUPPORT = NO\n")
            result = subprocess.run(["doxygen", "Doxyfile"], cwd=root, text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            self.assertEqual(0, result.returncode, result.stdout)
            self.assertNotRegex(result.stdout, r"(?i)warning:|error:")
            parsed = VisibleText()
            parsed.feed((root / "output/html/literal.html").read_text())
            parsed.close()
            text = "".join(parsed.text)
            for _, _, expected in patterns:
                self.assertIn(expected, text)
            self.assertNotRegex(text, r"&#(?:64|92|96);")


if __name__ == "__main__":
    unittest.main()
