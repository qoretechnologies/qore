#!/usr/bin/env python3
"""Regression tests for silent Doxygen link rendering failures.

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
CHECKER = ROOT / "doxygen/check-rendered-refs.py"
spec = importlib.util.spec_from_file_location("rendered_refs", CHECKER)
refs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(refs)


class RenderedReferencesTest(unittest.TestCase):
    def test_labels_symbols_intro_targets_and_line_numbers(self):
        html = '<p>text<br>\n@ref target "A label"\n\\ref Name::method\n@subpage peerintro</p>'
        self.assertEqual([(2, '@ref target "'), (3, r'\ref Name::method'),
                          (4, '@subpage peerintro')], refs.check_html(html))

    def test_literal_examples_and_directive_descriptions(self):
        for element in ('code', 'pre', 'tt', 'script', 'style'):
            self.assertEqual([], refs.check_html(
                f'<{element}>@ref target "label"</{element}>'))
        for classes in ('fragment', 'line', 'tt', 'other fragment'):
            self.assertEqual([], refs.check_html(
                f'<div class="{classes}"><span>@ref target "label"</span></div>'))
        self.assertEqual([], refs.check_html('<p>Resolve doxygen @ref tags in descriptions.</p>'))
        self.assertEqual([(1, '@ref target "')], refs.check_html(
            '<code>@ref example "label"</code><br/><p>@ref target "label"</p>'))
        self.assertEqual([(1, '@ref target "')], refs.check_html(
            '<p class>@ref target &quot;label&quot;</p>'))

    def test_markdown_links_split_by_autolinks_and_literal_boundaries(self):
        html = '<p>Intro\n[PROJ coordinate\ntransformation library](<a href="https://proj.org">https://proj.org</a>)</p>'
        self.assertEqual([(2, '[PROJ coordinate transformation library](https://proj.org)')],
                         refs.check_html(html))
        for target in ('https://example.com', 'http://example.com', '../peer/index.html#intro',
                       '#intro', 'guide.html'):
            with self.subTest(target=target):
                self.assertEqual(1, len(refs.check_html(f'<p>[Label]({target})</p>')))
                self.assertEqual([], refs.check_html(f'<p><a href="{target}">Label</a></p>'))
        for element in ('code', 'pre', 'tt', 'script', 'style'):
            self.assertEqual([], refs.check_html(f'<{element}>[Example](https://example.com)</{element}>'))
        self.assertEqual([], refs.check_html('<div class="fragment"><span>[Example](https://example.com)</span></div>'))
        self.assertEqual([], refs.check_html('<p>[Separate</p><p>blocks](https://example.com)</p>'))
        self.assertEqual([(1, '[Before{code}after](https://example.com)')],
                         refs.check_html('<p>[Before<code>literal</code>after](https://example.com)</p>'))
        self.assertEqual([], refs.check_html('<p>[Before<pre>literal</pre>after](https://example.com)</p>'))
        self.assertEqual([(1, '[Last](https://example.com)')], refs.check_html('[Last](https://example.com)'))

    def test_inline_markdown_and_explicit_literal_markup(self):
        self.assertEqual([(1, '`field`'), (2, '**required**')],
                         refs.check_html('<p>`field`\n**required**</p>'))
        self.assertEqual([(1, '`Namespace::method()`')], refs.check_html(
            '<p>`<a href="method.html">Namespace::method()</a>`</p>'))
        for element in ('code', 'pre', 'tt', 'script', 'style'):
            self.assertEqual([], refs.check_html(f'<{element}>`field` **required** ```cpp</{element}>'))
        self.assertEqual([], refs.check_html('<p><tt>field</tt> is <b>required</b>.</p>'))
        self.assertEqual([], refs.check_html('<span class="tt">`field` **required**</span>'))
        for element in ('code', 'tt', 'span class="tt"'):
            close = element.split()[0]
            self.assertEqual([(1, '**A {code} value**')], refs.check_html(
                f'<p>**A <{element}>`field` **literal**</{close}> value**</p>'))
        self.assertEqual([], refs.check_html('<p><b>A <tt>field</tt> value</b></p>'))
        self.assertEqual([], refs.check_html('<p>`separate</p><p>blocks`</p>'))
        self.assertEqual([(1, '```cpp'), (2, '```')], refs.check_html('<p>```cpp</p>\n<p>```</p>'))
        self.assertEqual([(2, '`first`'), (3, '`second`'), (4, '**third**')],
                         refs.check_html('<p>intro\n`first`\n`sec<a href="x">ond</a>`\n**third**</p>'))

    def test_cli_subpages_symlinks_and_missing_input(self):
        with tempfile.TemporaryDirectory(prefix='qore-rendered-refs-') as directory:
            root = Path(directory)
            (root / 'index.html').write_text('<a href="guide.html">Guide</a>')
            (root / 'guide.html').write_text('<p>@ref peerintro "Peer module"</p>')
            (root / 'alias').symlink_to(root, target_is_directory=True)
            result = subprocess.run([sys.executable, str(CHECKER), str(root)],
                                    text=True, capture_output=True, timeout=10)
            self.assertEqual(1, result.returncode)
            self.assertIn('guide.html:1:', result.stderr)
            self.assertIn('Checked 2 HTML files; 1 unprocessed', result.stdout)
            (root / 'guide.html').write_text('<p>[Peer](../Peer/html/index.html#peerintro)</p>')
            result = subprocess.run([sys.executable, str(CHECKER), str(root)],
                                    text=True, capture_output=True, timeout=10)
            self.assertEqual(1, result.returncode)
            self.assertIn('[Peer](../Peer/html/index.html#peerintro)', result.stderr)
            for markup in ('`field_name`', '**required**'):
                (root / 'guide.html').write_text(f'<p>{markup}</p>')
                result = subprocess.run([sys.executable, str(CHECKER), str(root)],
                                        text=True, capture_output=True, timeout=10)
                self.assertEqual(1, result.returncode)
                self.assertIn(markup, result.stderr)
            (root / 'guide.html').write_text('<a href="index.html#peerintro">Peer module</a>')
            result = subprocess.run([sys.executable, str(CHECKER), str(root)],
                                    text=True, capture_output=True, timeout=10)
            self.assertEqual(0, result.returncode, result.stderr)
            result = subprocess.run([sys.executable, str(CHECKER), str(root / 'missing')],
                                    text=True, capture_output=True, timeout=10)
            self.assertEqual(2, result.returncode)
            self.assertIn('does not exist', result.stderr)
            empty = root / 'empty'
            empty.mkdir()
            result = subprocess.run([sys.executable, str(CHECKER), str(empty)],
                                    text=True, capture_output=True, timeout=10)
            self.assertEqual(2, result.returncode)
            self.assertIn('no rendered HTML files', result.stderr)

    def test_real_doxygen_raw_html_and_safe_syntax(self):
        doxygen = os.environ.get('DOXYGEN_EXECUTABLE') or shutil.which('doxygen')
        if not doxygen:
            self.skipTest('Doxygen is required for the rendered documentation test')
        with tempfile.TemporaryDirectory(prefix='qore-doxygen-refs-') as directory:
            root = Path(directory)
            (root / 'pages.dox').write_text(r'''/** @mainpage Example
@section peerintro Peer introduction
@subpage guide
*/
/** @page guide Guide
@htmlonly @ref peerintro "Peer module" @endhtmlonly
@ref peerintro "A complete single-line link label"
[External reference](https://example.com)
`field_name` and **required**
An example: **A \c field_name value**
```cpp
int example = 42;
```
<tt>\\</tt> or <tt>/</tt>; HL7 sequence <tt>\\F\\</tt>.
@ref peerintro "After backslashes"
@par Config keys
From @ref peerintro "Reference below the paragraph title".
@code
// @ref peerintro "Intentional example"
@endcode
*/
''')
            config = root / 'Doxyfile'
            config.write_text(f'''PROJECT_NAME = Example
INPUT = "{root / 'pages.dox'}"
OUTPUT_DIRECTORY = "{root / 'output'}"
GENERATE_LATEX = NO
QUIET = YES
MARKDOWN_SUPPORT = NO
WARN_AS_ERROR = FAIL_ON_WARNINGS
''')
            result = subprocess.run([doxygen, str(config)], text=True,
                                    capture_output=True, timeout=30)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertEqual('', result.stderr)
            html = (root / 'output/html/guide.html').read_text()
            self.assertEqual(7, len(refs.check_html(html)))
            for label in ('A complete single-line link label', 'After backslashes',
                          'Reference below the paragraph title'):
                self.assertRegex(html, rf'href="[^"]*#peerintro"[^>]*>{label}</a>')
            self.assertRegex(html, r'<(?:code|span class="tt")>\\F\\</(?:code|span)>')
            source = root / 'pages.dox'
            source.write_text(source.read_text().replace(
                '@htmlonly @ref peerintro "Peer module" @endhtmlonly',
                '@ref peerintro "Peer module"').replace(
                '[External reference](https://example.com)',
                '<a href="https://example.com">External reference</a>').replace(
                '`field_name` and **required**', '<tt>field_name</tt> and <b>required</b>').replace(
                r'**A \c field_name value**', '<b>A <tt>field_name</tt> value</b>').replace(
                '```cpp\nint example = 42;\n```', '@code{.cpp}\nint example = 42;\n@endcode'))
            result = subprocess.run([doxygen, str(config)], text=True,
                                    capture_output=True, timeout=30)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertEqual('', result.stderr)
            safe_html = (root / 'output/html/guide.html').read_text()
            self.assertEqual([], refs.check_html(safe_html))
            self.assertRegex(safe_html, r'<(?:code|span class="tt")>field_name</(?:code|span)>')
            self.assertIn('<b>required</b>', safe_html)
            self.assertRegex(safe_html, r'href="https://example.com"[^>]*>External reference</a>')
            # A quoted backslash also silently consumes the following link commands.
            source.write_text(r'''/** @mainpage Example
@section peerintro Peer introduction
@subpage guide
*/
/** @page guide Guide
Directory separator: "\" or "/". @ref peerintro "After backslashes"
*/
''')
            result = subprocess.run([doxygen, str(config)], text=True,
                                    capture_output=True, timeout=30)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertEqual('', result.stderr)
            self.assertTrue(refs.check_html((root / 'output/html/guide.html').read_text()))


if __name__ == '__main__':
    unittest.main()
