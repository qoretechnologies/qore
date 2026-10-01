# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
import importlib.util
from pathlib import Path
import tempfile
import unittest

loader = importlib.util.spec_from_file_location('examples', Path(__file__).resolve().parents[1] / 'stage-examples.py')
examples = importlib.util.module_from_spec(loader)
loader.loader.exec_module(examples)


class ExamplesTest(unittest.TestCase):
    def test_content_interpreters_modes_and_development_exclusions(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); source = root / 'source'; source.mkdir()
            inputs = {'hello.qr': b'#!/usr/bin/env qore\nprintf("hello");\n',
                      'nested/modern.qr': b'#!/usr/bin/env qore\nprintf("modern");\n',
                      'nested/build.sh': b'#!/bin/bash\ntrue\n', 'data.bin': b'\x00\x01test',
                      'test/fixture/.keep': b'', '.gitignore': b'generated'}
            for name, data in inputs.items():
                p = source / name; p.parent.mkdir(parents=True, exist_ok=True); p.write_bytes(data); p.chmod(0o755)
            target = root / 'installed'; examples.stage(source, target)
            self.assertEqual({'hello.qr','nested/modern.qr','nested/build.sh','data.bin'},
                             {str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()})
            for name in ('hello.qr','nested/modern.qr'):
                self.assertTrue((target / name).read_bytes().startswith(b'#!/usr/bin/qore\n'))
                self.assertEqual(0o755, (target / name).stat().st_mode & 0o777)
            self.assertEqual(inputs['data.bin'], (target / 'data.bin').read_bytes())
            self.assertEqual(0o644, (target / 'data.bin').stat().st_mode & 0o777)
            self.assertEqual(inputs['nested/build.sh'], (target / 'nested/build.sh').read_bytes())
            self.assertEqual(inputs['hello.qr'], (source / 'hello.qr').read_bytes())
            with self.assertRaises(ValueError):
                examples.stage(source, target)

    def test_unsafe_or_unknown_inputs_leave_no_output(self):
        for kind in ('symlink', 'interpreter'):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as directory:
                root = Path(directory); source = root / 'source'; source.mkdir(); target = root / 'output'
                (source / 'safe.qr').write_text('#!/usr/bin/env qore\n')
                if kind == 'symlink':
                    (source / 'escape').symlink_to('/etc/passwd')
                else:
                    (source / 'unknown').write_text('#!/usr/bin/env unknown\n')
                with self.assertRaises(ValueError):
                    examples.stage(source, target)
                self.assertFalse(target.exists())

    def test_current_repository_examples(self):
        source = Path(__file__).resolve().parents[2] / 'examples'
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / 'examples'; examples.stage(source, target)
            self.assertFalse((target / 'test').exists())
            self.assertEqual('#!/usr/bin/qore', (target / 'HelloWorld.qr').read_text().splitlines()[0])
            self.assertGreater(len(list(target.rglob('*.qr'))), 10)
