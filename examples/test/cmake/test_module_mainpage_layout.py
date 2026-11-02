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
    def test_guide_references_must_link_to_the_guide_not_its_legacy_anchor(self):
        for href in ('index.html#oldnotes', '#oldnotes'):
            html = f'''<div class="textblock"><p><a href="{href}">Release notes</a></p>
<ul><li><a id="oldnotes"></a><a href="notes.html">Release notes</a></li></ul></div>'''
            self.assertEqual(['link targets a legacy guide navigation anchor: oldnotes'],
                             checker.inspect_mainpage(html))
            self.assertEqual([], checker.inspect_mainpage(html.replace(href, 'notes.html')))
            self.assertEqual([], checker.inspect_mainpage(html.replace(href, 'other.html#oldnotes')))

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
        html = html.replace('<p>Description.</p>', '<p><img src="app.svg" width="96"/>Description.</p>')
        self.assertEqual([], checker.inspect_mainpage(html, 'app.svg'))

    def test_logo_size_in_rendered_intro(self):
        page = '''<div class="textblock"><h1><a id="nativeintro"></a>Introduction</h1>
<p><img src="app.svg" {size}/>Description.</p></div>'''
        for size in ('width="96"', 'height="48"'):
            with self.subTest(size=size):
                self.assertEqual([], checker.inspect_mainpage(page.format(size=size), 'app.svg'))
        for size in ('', 'style="width:40%;"', 'width="96" style="width:40%;"', 'width="200"', 'width="96" height="96"'):
            with self.subTest(size=size):
                issues = checker.inspect_mainpage(page.format(size=size), 'app.svg')
                self.assertEqual(1, len(issues), issues)
                self.assertIn('a logo has width="96", or height="48"', issues[0])

    def test_logo_size_in_sources(self):
        source = '''/** @mainpage Example
    @htmlonly
    <div class="image">
      <img src="example-logo.svg" alt="Example Logo" style="width:40%;">
    </div>
    @endhtmlonly
    <img src="diagram.svg" style="width:80%;"/> a diagram is not a logo
    <img src="cid:logo123"> an email example is not a logo
*/'''
        issues = checker.inspect_logo_sources(source)
        self.assertEqual([(4, issues[0][1])], issues)
        self.assertIn('example-logo.svg', issues[0][1])
        for fixed in ('width="96"', 'height="48"'):
            with self.subTest(fixed=fixed):
                self.assertEqual([], checker.inspect_logo_sources(source.replace('style="width:40%;"', fixed)))
        # a logo found by its alt text, and a logo with no size
        self.assertEqual(1, len(checker.inspect_logo_sources('<img src="mark.svg" alt="Vendor logo"/>')))
        with tempfile.TemporaryDirectory() as directory:
            good = Path(directory) / 'good.qm'
            bad = Path(directory) / 'bad.qm'
            good.write_text(source.replace('style="width:40%;"', 'width="96"'), encoding='utf-8')
            bad.write_text(source, encoding='utf-8')
            result = subprocess.run([sys.executable, '-I', str(CHECKER), '--sources', str(good)], capture_output=True,
                                    text=True)
            self.assertEqual(0, result.returncode, result.stderr)
            result = subprocess.run([sys.executable, '-I', str(CHECKER), '--sources', str(good), str(bad)],
                                    capture_output=True, text=True)
            self.assertEqual(1, result.returncode)
            self.assertIn(f'{bad}:4:', result.stderr)

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
