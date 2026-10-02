#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Ensure URL documentation exports symbols consumed by external modules."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
QPP = os.environ.get("QORE_QPP_EXECUTABLE", shutil.which("qpp"))
DOXYGEN = os.environ.get("DOXYGEN", shutil.which("doxygen"))


@unittest.skipUnless(QPP and DOXYGEN, "qpp and doxygen are required")
class UrlDocIndexTest(unittest.TestCase):
    def test_url_symbols_and_external_links(self):
        with tempfile.TemporaryDirectory(prefix="qore-url-doc-index-") as directory:
            root = Path(directory)
            generated = root / "misc.dox.h"
            result = subprocess.run(
                [QPP, "--output=" + str(root / "misc.cpp"), "--dox-output=" + str(generated),
                 str(ROOT / "lib/ql_misc.qpp")], capture_output=True, text=True, timeout=60)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            text = generated.read_text()
            # Preserve the actual comments and declarations, isolating this regression
            # from unrelated documentation pages and their module dependencies.
            snippets = []
            for declaration in [r"string encode_url\([^;]+;", r"const RESOLVE_URL_ENCODE[^;]+;"]:
                match = re.search(declaration, text)
                self.assertIsNotNone(match, declaration)
                start = text.rfind("//!", 0, match.start())
                self.assertGreaterEqual(start, 0)
                snippets.append(text[start:match.end()])
            source = root / "url.h"
            source.write_text("//! Language namespace\nnamespace Qore {\n" + "\n".join(snippets) + "\n}\n")
            config = root / "Doxyfile"
            # Supporting references in the real comments are defined by the full
            # language manual. Resolve them here with minimal documented symbols.
            support = root / "support.dox"
            support.write_text("\n".join("/** @page " + name + " " + name + " */" for name in
                                         ["CONSTANT", "NAMED_ARGS", "True"]))
            config.write_text(f'''QUIET = YES
INPUT = "{source}" "{support}"
OUTPUT_DIRECTORY = "{root / 'output'}"
GENERATE_HTML = YES
GENERATE_LATEX = NO
GENERATE_TAGFILE = "{root / 'url.tag'}"
WARN_AS_ERROR = YES
WARN_IF_UNDOCUMENTED = NO
MARKDOWN_SUPPORT = YES
''')
            result = subprocess.run([DOXYGEN, str(config)], capture_output=True, text=True, timeout=60)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertNotIn("warning:", (result.stdout + result.stderr).lower())
            exported = {member.findtext("name") for member in ET.parse(root / "url.tag").iter("member")}
            self.assertIn("encode_url", exported)
            self.assertIn("RESOLVE_URL_ENCODE", exported)
            consumer = root / "consumer.dox"
            consumer.write_text("/** @page consumer External URL documentation\n"
                                "See @ref Qore::encode_url() and @ref Qore::RESOLVE_URL_ENCODE.\n*/\n")
            with config.open("a") as output:
                output.write(f'\nINPUT = "{consumer}"\nGENERATE_TAGFILE =\n'
                             f'TAGFILES = "{root / "url.tag"}=https://example.invalid/lang"\n')
            result = subprocess.run([DOXYGEN, str(config)], capture_output=True, text=True, timeout=60)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertNotIn("warning:", (result.stdout + result.stderr).lower())
            html = (root / "output/html/consumer.html").read_text()
            self.assertIn("https://example.invalid/lang/", html)
            # Negative control: the consumer really checks its external index.
            consumer.write_text("/** @page consumer Missing reference\n@ref missing_url_symbol\n*/\n")
            result = subprocess.run([DOXYGEN, str(config)], capture_output=True, text=True, timeout=60)
            self.assertNotEqual(0, result.returncode)
            self.assertIn("missing_url_symbol", result.stderr)


if __name__ == "__main__":
    unittest.main()
