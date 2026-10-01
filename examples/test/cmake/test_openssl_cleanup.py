#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Compile the legacy engine cleanup guard with available and disabled APIs."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]


class OpenSslCleanupTest(unittest.TestCase):
    def test_header_presence_does_not_imply_engine_api_availability(self):
        source = (ROOT / 'lib/qore-main.cpp').read_text()
        blocks = re.findall(r'^#if[^\n]*HAVE_OPENSSL_ENGINE_H[^\n]*\n'
                            r'\s*ENGINE_cleanup\(\);\n#endif', source, re.M)
        self.assertEqual(1, len(blocks))
        with tempfile.TemporaryDirectory(prefix='qore-openssl-cleanup-') as temporary:
            root = Path(temporary)
            header = root / 'openssl'
            header.mkdir()
            (header / 'engine.h').write_text(
                '#if !defined(OPENSSL_NO_ENGINE) && OPENSSL_VERSION_NUMBER < 0x10100000L\n'
                'static void ENGINE_cleanup() { ++calls; }\n'
                '#endif\n')
            probe = root / 'probe.cpp'
            probe.write_text('static int calls = 0;\n'
                             '#ifdef HAVE_OPENSSL_ENGINE_H\n#include <openssl/engine.h>\n#endif\n'
                             'int main() {\n' + blocks[0] + '\nreturn calls;\n}\n')
            for name, definitions, expected in [
                    ('legacy', ['HAVE_OPENSSL_ENGINE_H', 'OPENSSL_VERSION_NUMBER=0x10002000L'], 1),
                    ('legacy-disabled', ['HAVE_OPENSSL_ENGINE_H', 'OPENSSL_NO_ENGINE',
                                         'OPENSSL_VERSION_NUMBER=0x10002000L'], 0),
                    ('no-header', ['OPENSSL_VERSION_NUMBER=0x10002000L'], 0),
                    ('automatic-cleanup', ['HAVE_OPENSSL_ENGINE_H', 'OPENSSL_VERSION_NUMBER=0x10100000L'], 0),
                    ('empty-engine-header', ['HAVE_OPENSSL_ENGINE_H', 'OPENSSL_VERSION_NUMBER=0x30500000L'], 0)]:
                with self.subTest(name=name):
                    exe = root / name
                    result = subprocess.run(['c++', '-Wall', '-Wextra', '-Werror', '-I', str(root),
                                             *['-D' + d for d in definitions], str(probe), '-o', str(exe)],
                                            capture_output=True, text=True)
                    self.assertEqual(0, result.returncode, result.stderr)
                    self.assertEqual(expected, subprocess.run([str(exe)], check=False).returncode)


if __name__ == '__main__':
    unittest.main()
