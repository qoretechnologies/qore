#!/usr/bin/env python3
# Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
"""Compile and execute the real operator template classes with strict address diagnostics."""
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
BUILD = Path(os.environ.get('QORE_TEST_BUILD_DIR', ROOT / 'build')).resolve()
INCLUDE = Path(os.environ.get('QORE_TEST_INCLUDE_DIR', ROOT / 'include')).resolve()
HEADER = 'qore/intern/QoreOperatorNode.h'


class OperatorRootEffectTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.environ.get('QORE_TEST_OUTPUT_DIR'):
            cls.output = Path(os.environ['QORE_TEST_OUTPUT_DIR']).resolve()
            cls.output.mkdir(parents=True)
        else:
            temporary = tempfile.TemporaryDirectory(prefix='qore-operator-effects-')
            cls.addClassCleanup(temporary.cleanup)
            cls.output = Path(temporary.name)
        cls.receipts = []
        body = re.search(r'^int AbstractQoreNode::parseInit\([^\n]*\{\n.*?^\}',
            (ROOT / 'lib/AbstractQoreNode.cpp').read_text(), re.M | re.S)
        if body is None:
            raise AssertionError('missing actual default parse implementation')
        (cls.output / 'operator-base-parse.inc').write_text(body.group(0) + '\n')
        text = (ROOT / 'include' / HEADER).read_text()
        original, count = re.subn(r'        if constexpr \(std::is_convertible_v<const T\*, const LValueOperatorNode\*>\) \{\n'
            r'            return true;\n        \} else \{\n'
            r'            return dynamic_cast<const LValueOperatorNode\*>\(this\) != nullptr;\n        \}',
            '        return dynamic_cast<const LValueOperatorNode*>(this);', text)
        if count != 4:
            raise AssertionError('expected all four operator template bodies')
        for name, data in [('baseline', original), ('fixed', text)]:
            path = cls.output / name / HEADER
            path.parent.mkdir(parents=True)
            path.write_text(data)

    @classmethod
    def invoke(cls, name, command):
        p = subprocess.run(command, capture_output=True, text=True)
        (cls.output / (name + '.stdout')).write_text(p.stdout)
        (cls.output / (name + '.stderr')).write_text(p.stderr)
        cls.receipts.append({'name': name, 'command': command, 'exit_code': p.returncode})
        (cls.output / 'commands.json').write_text(json.dumps(cls.receipts, indent=2) + '\n')
        return p

    def test_real_operator_classes(self):
        # This focused internal-header consumer checks the address diagnostic.
        # The RPM's normal compiler flags and complete package builds are separate gates.
        flags = shlex.split(os.environ.get('QORE_TEST_CXXFLAGS', '-O3 -g -DNDEBUG'))
        compiler = shlex.split(os.environ.get('CXX', 'c++'))
        common = [*compiler, *flags, '-std=c++20', '-DHAVE_UNIX_CONFIG_H', '-Werror=address']
        includes = ['-I' + str(BUILD / 'include'), '-I' + str(INCLUDE), '-I' + str(BUILD),
                    '-I' + str(self.output)]
        fixture = ROOT / 'examples/test/cmake/operator_root_effect.cpp'
        bad = self.invoke('baseline', [*common, '-I' + str(self.output / 'baseline'), *includes,
            '-c', str(fixture), '-o', str(self.output / 'baseline.o')])
        self.assertNotEqual(0, bad.returncode)
        errors = [line for line in bad.stderr.splitlines() if 'error:' in line]
        self.assertEqual(8, len(errors), bad.stderr)
        self.assertTrue(all('[-Werror=address]' in line for line in errors), bad.stderr)
        binary = self.output / 'operator-root-effect'
        good = self.invoke('fixed-compile', [*common, '-I' + str(self.output / 'fixed'), *includes,
            str(fixture), '-L' + str(BUILD), '-Wl,-rpath,' + str(BUILD), '-lqore', '-o', str(binary)])
        self.assertEqual(0, good.returncode, good.stderr)
        self.assertEqual('', good.stderr)
        run = self.invoke('fixed-run', [str(binary)])
        self.assertEqual(0, run.returncode, run.stderr)
        self.assertEqual('PASS: 449 operator effect checks\n', run.stdout)
        self.assertEqual('', run.stderr)
        if os.environ.get('QORE_TEST_VALGRIND') == '1':
            vg = self.invoke('fixed-valgrind', ['valgrind', '--error-exitcode=99', '--leak-check=full',
                '--show-leak-kinds=definite,indirect,possible', '--errors-for-leak-kinds=definite,indirect,possible',
                str(binary)])
            self.assertEqual(0, vg.returncode, vg.stderr)
            self.assertIn('ERROR SUMMARY: 0 errors', vg.stderr)
            for kind in ('definitely', 'indirectly', 'possibly'):
                self.assertRegex(vg.stderr, kind + r' lost:\s+0 bytes in 0 blocks')


if __name__ == '__main__':
    unittest.main()
