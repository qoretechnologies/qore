#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Build Qore's actual Flex target declaration and verify its DWARF sources."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


REPOSITORY = Path(__file__).resolve().parents[2]


class FlexSourcePathsTest(unittest.TestCase):
    def test_scanner_sources_resolve_from_out_of_tree_build(self):
        cmake = (REPOSITORY / 'CMakeLists.txt').read_text()
        target = re.search(r'(?m)^flex_target\(qorescanner\s+[^)]+\)', cmake)
        self.assertIsNotNone(target)
        minimum = re.search(r'(?m)^cmake_minimum_required\([^)]+\)', cmake)
        self.assertIsNotNone(minimum)
        env = os.environ.copy()
        for key in ('LD_PRELOAD', 'LD_LIBRARY_PATH'):
            env.pop(key, None)
        with tempfile.TemporaryDirectory(prefix='qore flex sources ') as temporary:
            root = Path(temporary)
            (root / 'lib').mkdir()
            (root / 'lib/scanner.lpp').write_text(r'''%option noyywrap
%{
#include <stdio.h>
static int numbers = 0, words = 0;
%}
%%
[0-9]+       { ++numbers; }
[a-zA-Z]+    { ++words; }
[ \t\n]+     {}
.            {}
%%
int main(void) {
    yylex();
    yylex_destroy();
    printf("%d %d\n", numbers, words);
    return 0;
}
''')
            (root / 'CMakeLists.txt').write_text(minimum[0] + '''
project(QoreFlexSourcePaths LANGUAGES CXX)
find_package(FLEX REQUIRED)
''' + target[0] + '''
add_executable(scanner ${FLEX_qorescanner_OUTPUTS})
''')

            def run(command, **options):
                result = subprocess.run(command, cwd=root, env=env, text=True,
                                        capture_output=True, **options)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertNotRegex(result.stdout + result.stderr,
                                    r'(?im)(?:warning:|error:|CMake Warning)')
                return result

            run(['cmake', '-S', str(root), '-B', str(root / 'build'),
                 '-DCMAKE_BUILD_TYPE=Debug'])
            run(['cmake', '--build', str(root / 'build'), '--parallel', '2'])
            binary = root / 'build/scanner'
            for source, expected in [('', '0 0\n'), ('123 abc 34!\n', '2 1\n'),
                                     ('!!\n', '0 0\n'), ('\u017elu\u0165 123\n', '1 1\n')]:
                with self.subTest(source=source):
                    self.assertEqual(expected, run([str(binary)], input=source).stdout)
            listing = root / 'debug-sources.list'
            run(['debugedit', '-b', str(root), '-d', '/usr/src/debug/qore-flex-test',
                 '-l', str(listing), str(binary)])
            sources = {name for name in listing.read_bytes().decode().split('\0')
                       if name and not name.endswith('/')}
            self.assertIn('lib/scanner.lpp', sources)
            self.assertIn('build/scanner.cpp', sources)
            for name in sources:
                self.assertTrue((root / name).is_file(), 'Missing debug source: ' + name)
            # Exercise the generated scanner's allocation/teardown path as well.
            result = run(['valgrind', '--error-exitcode=99', '--leak-check=full',
                          '--show-leak-kinds=all', '--errors-for-leak-kinds=all',
                          str(binary)], input='word 123\n' * 1000)
            self.assertEqual('1000 1000\n', result.stdout)
            self.assertIn('All heap blocks were freed -- no leaks are possible', result.stderr)
            self.assertIn('ERROR SUMMARY: 0 errors', result.stderr)


if __name__ == '__main__':
    unittest.main()
