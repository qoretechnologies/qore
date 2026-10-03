#!/usr/bin/env python3
"""Validate the module initial-release documentation policy.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
CHECKER = ROOT / 'doxygen/check-module-release-notes.py'
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('release_notes', CHECKER)
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class ModuleReleaseNotesTest(unittest.TestCase):
    def test_initial_and_subsequent_releases(self):
        source = '''/** @page history Release Notes
    @section v2 Version 2.0
    - added a feature
    @section v1 Version 1.0
    - initial release
*/'''
        self.assertEqual([], checker.inspect_source(source))
        self.assertEqual([], checker.inspect_source(source.replace('@', '\\')))
        self.assertEqual([], checker.inspect_source(source.replace('@section', '@subsection')))

    def test_initial_release_rejects_features_and_development_fixes(self):
        for entry in ('- Initial release', '- initial public release',
                      '- initial release\n    - supports streaming',
                      '- fixed a crash\n- initial release',
                      'Initial release, shipped with Qore 3.0.'):
            with self.subTest(entry=entry):
                source = '/** @page history Release Notes\n    @section v1 Version 1.0\n' + entry + '\n*/'
                issues = checker.inspect_source(source)
                self.assertEqual(1, len(issues), issues)
                self.assertIn('v1: initial release', issues[0])

    def test_preamble_and_comment_boundaries(self):
        source = '''/** @page history Release Notes
    - fixed a crash
    @section v1 Version 1.0
    - initial release
*/
string value = "initial release with features";
/** @brief unrelated API documentation */'''
        self.assertEqual(['release notes contain bullets before the first release section'],
                         checker.inspect_source(source))
        self.assertEqual([], checker.inspect_source(source.replace('    - fixed a crash\n', '')))

    def test_release_section_preamble_and_nested_module_announcements(self):
        source = """/** @mainpage Example
    @section intro Introduction
    Overview.
    @section relnotes Example Release Notes
    - fixed development behavior
    @subsection v1 Version 1.0
    - initial release
*/"""
        self.assertEqual(['release notes contain bullets before the first release section'],
                         checker.inspect_source(source))
        self.assertEqual([], checker.inspect_source(source.replace('    - fixed development behavior\n', '')))
        announcements = """/** @page history Release Notes
    @section v2 Version 2.0
    - added a module
      - initial release: provides streaming
    - fixed a regression introduced in the initial release
*/"""
        self.assertEqual([], checker.inspect_source(announcements))

    def test_cli_reports_files_and_errors(self):
        with tempfile.TemporaryDirectory(prefix='qore-release-notes-') as directory:
            source = Path(directory) / 'module.dox.tmpl'
            source.write_text('/** @section v1 Version 1.0\n- initial release\n*/')
            command = [sys.executable, str(CHECKER), str(source)]
            result = subprocess.run(command, capture_output=True, text=True, timeout=10)
            self.assertEqual(0, result.returncode, result.stderr)
            source.write_text(source.read_text().replace('- initial release', '- initial release with features'))
            result = subprocess.run(command, capture_output=True, text=True, timeout=10)
            self.assertEqual(1, result.returncode)
            self.assertIn(str(source), result.stderr)
            result = subprocess.run(command + [str(source.with_name('missing'))],
                                    capture_output=True, text=True, timeout=10)
            self.assertEqual(1, result.returncode)
            self.assertIn('missing', result.stderr)


if __name__ == '__main__':
    unittest.main()
