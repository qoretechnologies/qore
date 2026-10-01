# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
loader = importlib.util.spec_from_file_location('licenses', ROOT / 'rpm/stage-licenses.py')
licenses = importlib.util.module_from_spec(loader)
loader.loader.exec_module(licenses)


class LicenseTest(unittest.TestCase):
    def test_actual_notices_are_complete_without_implementation(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory); licenses.stage(ROOT, output)
            self.assertEqual({'linenoise.LICENSE','wcwidth.LICENSE'}, {p.name for p in output.iterdir()})
            for name, marker in licenses.NOTICES.items():
                data = (ROOT / 'modules/linenoise/src/linenoise' / (name + '.cpp')).read_text()
                notice = (output / (name + '.LICENSE')).read_text()
                self.assertEqual(data[:data.index('*/') + 2] + '\n', notice)
                self.assertIn(marker, notice)
                self.assertNotIn('#include', notice)
            self.assertIn('Markus Kuhn -- 2007-05-26', (output / 'wcwidth.LICENSE').read_text())
            self.assertIn('Copyright (C) 2026', (output / 'linenoise.LICENSE').read_text())

    def test_invalid_header_or_missing_grant_is_rejected(self):
        for data in ('int main() {}', '/* unterminated', '/* ordinary comment */\nPermission elsewhere'):
            with self.subTest(data=data), self.assertRaises(ValueError):
                licenses.extract_notice(data, 'Permission')

    def test_missing_source_leaves_no_output(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'output'; output.mkdir()
            with self.assertRaises(FileNotFoundError):
                licenses.stage(directory, output)
            self.assertEqual([], list(output.iterdir()))
