#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Compile the real operator page and consume its exported cross-reference index."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
QPP = os.environ.get('QORE_QPP_EXECUTABLE', shutil.which('qpp'))


@unittest.skipUnless(QPP and shutil.which('doxygen'), 'requires qpp and Doxygen')
class OperatorIndexTest(unittest.TestCase):
    def test_operator_page_exports_links_consumable_by_external_modules(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'operators.dox'
            subprocess.run([QPP, '--table-strict', '--table=' + str(ROOT / 'doxygen/lang/180_operators.dox.tmpl'),
                            '--output=' + str(source)], check=True, capture_output=True)
            config = root / 'Doxyfile'
            # Other language chapters are intentionally absent in this focused
            # fixture. Syntax diagnostics remain enabled; links are checked below.
            config.write_text('QUIET = YES\nINPUT = operators.dox\nOUTPUT_DIRECTORY = output\n'
                              'WARN_IF_DOC_ERROR = NO\nWARN_AS_ERROR = YES\nGENERATE_LATEX = NO\n'
                              'GENERATE_TAGFILE = operators.tag\n')
            result = subprocess.run(['doxygen', str(config)], cwd=root, capture_output=True, text=True)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertNotIn('warning:', result.stderr.lower())
            tree = ET.parse(root / 'operators.tag')
            pages = [c for c in tree.findall('compound') if c.findtext('name') == 'operators']
            self.assertEqual(1, len(pages))
            anchors = {a.text for a in pages[0].findall('docanchor')}
            self.assertTrue({'background', 'backquote_operator', 'bitwise_or_operator'} <= anchors, anchors)
            page = root / 'output/html/operators.html'
            self.assertTrue(page.is_file())
            consumer = root / 'consumer.dox'
            consumer.write_text('/** @page consumer Consumer\n'
                                '@ref background "background launch" and '
                                '@ref bitwise_or_operator "binary or".\n*/\n')
            config.write_text('QUIET = YES\nINPUT = consumer.dox\nOUTPUT_DIRECTORY = consumer\n'
                              'WARN_AS_ERROR = YES\nGENERATE_LATEX = NO\n'
                              'TAGFILES = "operators.tag=https://example.invalid/lang"\n')
            result = subprocess.run(['doxygen', str(config)], cwd=root, capture_output=True, text=True)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertNotIn('warning:', result.stderr.lower())
            html = (root / 'consumer/html/consumer.html').read_text()
            self.assertIn('https://example.invalid/lang/operators.html#background', html)
            self.assertIn('https://example.invalid/lang/operators.html#bitwise_or_operator', html)


if __name__ == '__main__':
    unittest.main()
