#!/usr/bin/env python3
"""Keep imported documentation out of navigation while retaining cross-references.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from html.parser import HTMLParser
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get('CMAKE_EXECUTABLE', 'cmake')
DOXYGEN = os.environ.get('DOXYGEN_EXECUTABLE') or shutil.which('doxygen')
TEMPLATES = (
    'doxygen/Doxyfile.in',
    'doxygen/lang/Doxyfile.in', 'doxygen/lang/Doxyfile.tmpl',
    'doxygen/lib/Doxyfile.in', 'doxygen/lib/Doxyfile.tmpl',
    'doxygen/modules/Doxyfile.in', 'doxygen/modules/Doxyfile.cmake.in',
    'doxygen/qlib/Doxyfile.in', 'doxygen/qlib/Doxyfile.tmpl',
    'doxygen/qlib/Doxyfile.cmake.tmpl',
)


class Links(HTMLParser):
    def __init__(self, text):
        super().__init__()
        self.hrefs = []
        self.feed(text)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == 'a' and 'href' in attrs:
            self.hrefs.append(attrs['href'])


class ModuleDocNavigationTest(unittest.TestCase):
    def run_command(self, command, cwd):
        result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=60)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertEqual('', result.stderr)

    def test_templates_disable_imported_page_and_group_indexes(self):
        for template in TEMPLATES:
            for setting in ('EXTERNAL_PAGES', 'EXTERNAL_GROUPS'):
                with self.subTest(template=template, setting=setting):
                    values = re.findall(rf'^{setting}\s*=\s*(\S+)',
                                        (ROOT / template).read_text(), re.MULTILINE)
                    self.assertEqual(['NO'], values)

    @unittest.skipUnless(DOXYGEN, 'Doxygen is required')
    def test_external_references_resolve_without_importing_navigation(self):
        for destination in ('https://example.invalid/foreign', '../../foreign/html'):
            with self.subTest(destination=destination), tempfile.TemporaryDirectory(
                    prefix='qore module navigation ') as directory:
                root = Path(directory)
                source = root / 'source'
                (source / 'docs').mkdir(parents=True)
                shutil.copyfile(ROOT / 'doxygen/footer_template.html', source / 'docs/footer_template.html')
                (root / 'foreign.h').write_text('''/** @page foreign_guide Foreign Guide
@section foreignintro Foreign Introduction
Imported guide body.
*/
/** @defgroup foreign_group Foreign Group
Imported group body.
*/
/** Imported API class. */
class ForeignApi {};
''')
                (root / 'foreign.Doxyfile').write_text(f'''INPUT = "{root}/foreign.h"
GENERATE_HTML = NO
GENERATE_LATEX = NO
GENERATE_TAGFILE = "{root}/foreign.tag"
QUIET = YES
WARN_AS_ERROR = YES
''')
                self.run_command([DOXYGEN, 'foreign.Doxyfile'], root)
                (source / 'local.h').write_text('''/** @mainpage Local Module

@tableofcontents

@section localintro Local Introduction

See @ref foreignintro "Imported module", @ref ForeignApi "Imported API",
and @ref foreign_group "Imported group".

- @subpage local_guide
*/
/** @page local_guide Local Guide
Local guide body.
*/
/** @page local_reference Local Reference
Local reference body.
*/
/** @defgroup local_group Local Group
Local group body.
*/
''')
                (root / 'configure.cmake').write_text(f'''include("{ROOT}/cmake/QoreMacros.cmake")
set(CMAKE_SOURCE_DIR "{source}")
set(CMAKE_BINARY_DIR "{root}")
set(module_name local)
set(CURRENT_MODULE_NAME local)
set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)
set(_dox_input "\\\"{source}/local.h\\\"")
set(_dox_output "{root}/output")
set(QORE_DOXYGEN_TAGFILE "{root}/foreign.tag")
set(QORE_DOXYGEN_TAG_URL "{destination}")
qore_configure_module_doxygen("{ROOT}/doxygen/Doxyfile.in" "{root}/Doxyfile")
file(APPEND "{root}/Doxyfile" "\\nWARN_AS_ERROR = YES\\n")
''')
                self.run_command([CMAKE, '-P', 'configure.cmake'], root)
                self.run_command([DOXYGEN, 'Doxyfile'], root)
                html = root / 'output/html'
                navigation = ('navtreedata.js', 'index.js', 'pages.js', 'modules.js', 'topics.js')
                nav = '\n'.join((html / name).read_text() for name in navigation if (html / name).exists())
                self.assertIn('local_reference.html', nav)
                self.assertIn('group__local__group.html', nav)
                self.assertIn('local_reference.html', (html / 'pages.html').read_text())
                for filename in (*navigation, 'pages.html', 'modules.html', 'topics.html'):
                    path = html / filename
                    if path.exists():
                        text = path.read_text()
                        self.assertNotIn(destination, text, filename)
                        self.assertNotIn('Foreign Guide', text, filename)
                        self.assertNotIn('Foreign Group', text, filename)
                targets = {compound.findtext('name'): compound.findtext('filename')
                           for compound in ET.parse(root / 'foreign.tag').findall('compound')}
                links = Links((html / 'index.html').read_text()).hrefs
                self.assertIn('local_guide.html', links)
                self.assertTrue((html / 'local_guide.html').is_file())
                for name, anchor in (('foreign_guide', '#foreignintro'), ('ForeignApi', ''), ('foreign_group', '')):
                    filename = targets[name]
                    if not filename.endswith('.html'):
                        filename += '.html'
                    self.assertIn(f'{destination}/{filename}{anchor}', links)


if __name__ == '__main__':
    unittest.main()
