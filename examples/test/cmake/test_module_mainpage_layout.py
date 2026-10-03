#!/usr/bin/env python3
"""Check concise guide navigation, stable anchors, and module intro logos.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
CHECKER = ROOT / 'doxygen/check-module-mainpage.py'
spec = importlib.util.spec_from_file_location('mainpages', CHECKER)
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class MainpageLayoutTest(unittest.TestCase):
    def test_rejects_heading_link_pairs_but_preserves_substantive_sections(self):
        for level in ('h1', 'h2'):
            html = f'<div class="textblock"><{level}>Architecture</{level}><p><a href="guide.html">Architecture</a></p></div>'
            self.assertEqual(1, len(checker.inspect_mainpage(html)))
            self.assertEqual([], checker.inspect_mainpage(html.replace('</p>', ' — component relationships</p>')))
        self.assertEqual(['missing mainpage content'], checker.inspect_mainpage('<html></html>'))

    def test_logo_must_be_in_intro_not_in_companion_list(self):
        html = '''<div class="textblock"><h1><a id="nativeintro"></a>Introduction</h1>
<p>Description.</p><h1>Companion modules</h1><ul><li><img src="app.svg"/>Provider</li></ul></div>'''
        self.assertEqual(1, len(checker.inspect_mainpage(html, 'app.svg')))
        html = html.replace('<p>Description.</p>', '<p><img src="app.svg"/>Description.</p>')
        self.assertEqual([], checker.inspect_mainpage(html, 'app.svg'))

    def test_rendered_navigation_anchors_and_exported_logo(self):
        doxygen = os.environ.get('DOXYGEN_EXECUTABLE') or shutil.which('doxygen')
        if not doxygen:
            self.skipTest('Doxygen is required')
        with tempfile.TemporaryDirectory(prefix='qore-mainpage-layout-') as directory:
            root = Path(directory)
            logo = root / 'app.svg'
            logo.write_text('<svg xmlns="http://www.w3.org/2000/svg" width="96" height="96"><circle cx="48" cy="48" r="40"/></svg>')
            (root / 'main.dox').write_text('''/** @mainpage Native module
@section nativeintro Introduction
<img src="app.svg" width="96" alt="Module logo"/>
Description.
@section nativedocumentation Documentation
- @anchor oldarchitecture @subpage architecture — component relationships
- @anchor oldexamples @subpage examples — common operations
*/
/** @page architecture Architecture
Architecture details.
*/
/** @page examples Examples
Example details.
*/
''')
            config = root / 'Doxyfile'
            config.write_text(f'''PROJECT_NAME = Native
INPUT = "{root / 'main.dox'}"
OUTPUT_DIRECTORY = "{root / 'output'}"
HTML_EXTRA_FILES = "{logo}"
GENERATE_LATEX = NO
QUIET = YES
WARN_AS_ERROR = FAIL_ON_WARNINGS
''')
            result = subprocess.run([doxygen, str(config)], text=True, capture_output=True, timeout=30)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertEqual('', result.stderr)
            page = root / 'output/html/index.html'
            html = page.read_text()
            self.assertEqual([], checker.inspect_mainpage(html, 'app.svg'))
            for anchor in ('nativeintro', 'oldarchitecture', 'oldexamples'):
                self.assertIn(f'id="{anchor}"', html)
            for name in ('architecture', 'examples'):
                self.assertIn(f'href="{name}.html"', html)
                self.assertTrue((page.parent / f'{name}.html').is_file())
            self.assertEqual(logo.read_bytes(), (page.parent / 'app.svg').read_bytes())
            result = subprocess.run([sys.executable, str(CHECKER), '--logo', 'app.svg', str(page)],
                                    text=True, capture_output=True, timeout=10)
            self.assertEqual(0, result.returncode, result.stderr)
            (page.parent / 'app.svg').unlink()
            result = subprocess.run([sys.executable, str(CHECKER), '--logo', 'app.svg', str(page)],
                                    text=True, capture_output=True, timeout=10)
            self.assertEqual(1, result.returncode)
            self.assertIn('missing exported logo', result.stderr)


if __name__ == '__main__':
    unittest.main()
