#!/usr/bin/env python3
"""Validate the structure checker of the Qore language release notes.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
CHECKER = ROOT / 'doxygen/check-release-notes.py'
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('check_release_notes', CHECKER)
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)

GOOD = '''/** @page release_notes Release Notes

    @tableofcontents

    @section qore_3_1_0 Qore 3.1.0

    @par Release Summary
    @parblock
    Major release; highlights:
    - faster
    @endparblock

    @subsection qore_3_1_0_compatibility Changes That Can Affect Backwards-Compatibility
    @anchor qore_3_1_0_some_change
    @par A Changed Behavior
    @parblock
    The first paragraph.

    The second paragraph.
    @code{.py}
string s = "x";

my code
    @endcode
    @endparblock

    @par A Short Change
    One paragraph after a heading without a block.

    @subsection qore_3_1_0_new_features New Features in Qore
    - a new feature
      continued

      with a nested paragraph
    - another feature

    @subsection qore_3_1_0_updated_modules Updated Modules
    - @ref restclientintro "RestClient" module
      - an update

    @subsection qore_3_1_0_bug_fixes Bug Fixes in Qore
    - fixed a bug

    @section qore_3_0_0 Qore 3.0.0

    @par Release Summary
    Bugfix release

    @subsection qore_3_0_0_bug_fixes Bug Fixes in Qore
    - fixed a bug

    @section qore_0947 Qore 0.9.4.7

    @par Release Summary

    Bugfix release; see details below

    @subsection qore_0947_bug_fixesd Bug Fixes in Qore
    - a legacy section keeps its historical subsection id

    unchecked legacy paragraph
*/
'''


def messages(source):
    return [message for _, message in checker.inspect_source(source)]


class ReleaseNotesStructureTest(unittest.TestCase):
    def test_valid_structure(self):
        self.assertEqual([], checker.inspect_source(GOOD))
        # backslash commands are equivalent to at-sign commands
        self.assertEqual([], checker.inspect_source(GOOD.replace('@', '\\').replace('\\ref', '@ref')))

    def test_text_before_summary(self):
        source = GOOD.replace('    @section qore_3_1_0 Qore 3.1.0\n',
                              '    @section qore_3_1_0 Qore 3.1.0\n\n    A stray note about a fix.\n')
        issues = checker.inspect_source(source)
        self.assertEqual(1, len(issues), issues)
        self.assertIn('qore_3_1_0: the section must begin with "@par Release Summary"', issues[0][1])
        self.assertEqual(source.split('\n').index('    A stray note about a fix.') + 1, issues[0][0])

    def test_text_before_summary_in_legacy_section(self):
        source = GOOD.replace('    @section qore_0947 Qore 0.9.4.7\n',
                              '    @section qore_0947 Qore 0.9.4.7\n    @note a note\n')
        self.assertEqual(['qore_0947: the section must begin with "@par Release Summary"; release notes go in the '
                          'summary or in a subsection, never before the summary'], messages(source))

    def test_missing_summary(self):
        source = GOOD.replace('''    @par Release Summary
    Bugfix release
''', '')
        self.assertEqual(1, len(messages(source)))
        self.assertIn('qore_3_0_0: the section must begin', messages(source)[0])

    def test_text_after_summary(self):
        for summary_end in ('    @endparblock\n', '    Bugfix release\n'):
            with self.subTest(summary_end=summary_end):
                source = GOOD.replace(summary_end, summary_end + '\n    An appended paragraph.\n', 1)
                issues = messages(source)
                self.assertEqual(1, len(issues), issues)
                self.assertIn('text after the release summary and before the first subsection', issues[0])

    def test_loose_paragraph_in_subsection(self):
        source = GOOD.replace('    @subsection qore_3_1_0_bug_fixes Bug Fixes in Qore\n',
                              '    @subsection qore_3_1_0_bug_fixes Bug Fixes in Qore\n    A loose fix.\n\n')
        self.assertEqual(['qore_3_1_0: qore_3_1_0_bug_fixes: loose paragraph; every entry must be a bullet ("- ...") '
                          'or a "@par Title" block'], messages(source))
        # a second paragraph of a @par without @parblock is detached from its heading
        source = GOOD.replace('One paragraph after a heading without a block.\n',
                              'One paragraph after a heading without a block.\n\n    A detached paragraph.\n')
        self.assertEqual(1, len(messages(source)))
        self.assertIn('qore_3_1_0_compatibility: loose paragraph', messages(source)[0])
        # an anchor must label an entry
        source = GOOD.replace('    - fixed a bug\n\n    @section qore_3_0_0',
                              '    - fixed a bug\n\n    @anchor qore_3_1_0_x\n    A loose fix.\n\n    @section qore_3_0_0')
        self.assertEqual(1, len(messages(source)))
        self.assertIn('qore_3_1_0_bug_fixes: loose paragraph', messages(source)[0])

    def test_unterminated_region(self):
        source = GOOD.replace('    @endcode\n    @endparblock\n', '    @endcode\n', 1)
        issues = checker.inspect_source(source)
        self.assertEqual(1, len(issues), issues)
        self.assertEqual('@parblock without @endparblock in its section or subsection', issues[0][1])
        self.assertEqual(source.split('\n').index('    @parblock', 10) + 1, issues[0][0])
        source = GOOD.replace('    @endparblock\n', '', 1)
        self.assertEqual(['@parblock without @endparblock in its section or subsection'], messages(source))

    def test_subsection_ids_and_order(self):
        source = GOOD.replace('qore_3_1_0_updated_modules Updated', 'qore_3_1_0_modules Updated')
        self.assertEqual(1, len(messages(source)))
        self.assertIn('subsection "qore_3_1_0_modules" is not one of the standard subsections', messages(source)[0])
        source = GOOD.replace('qore_3_1_0_updated_modules Updated', 'qore_3_0_0_updated_modules Updated')
        self.assertIn('is not one of the standard subsections', messages(source)[0])
        # bug_fixes before updated_modules, and then the real bug_fixes repeats it
        source = GOOD.replace('qore_3_1_0_new_features New', 'qore_3_1_0_bug_fixes New')
        issues = checker.inspect_source(source)
        self.assertEqual(2, len(issues), issues)
        lines = source.split('\n')
        self.assertEqual([lines.index('    @subsection qore_3_1_0_updated_modules Updated Modules') + 1,
                          lines.index('    @subsection qore_3_1_0_bug_fixes Bug Fixes in Qore') + 1],
                         [number for number, _ in issues])
        self.assertIn('"qore_3_1_0_updated_modules" is out of order or repeated', issues[0][1])
        self.assertIn('"qore_3_1_0_bug_fixes" is out of order or repeated', issues[1][1])
        source = GOOD.replace('qore_3_1_0_compatibility Changes', 'qore_3_1_0_new_features Changes')
        self.assertEqual(1, len(messages(source)))
        self.assertIn('subsection "qore_3_1_0_new_features" is out of order or repeated', messages(source)[0])

    def test_quote_trap(self):
        source = GOOD.replace('    - fixed a bug\n\n    @section qore_3_0_0',
                              '    - a field such as <tt>"C:\\\\data\\\\"</tt> is read\n\n    @section qore_3_0_0')
        issues = checker.inspect_source(source)
        self.assertEqual(1, len(issues), issues)
        self.assertIn('a backslash before a double quote', issues[0][1])
        self.assertEqual([], checker.inspect_source(source.replace('\\\\"', '&quot;')))

    def test_empty_code_span(self):
        for word in ('.qm', '?:', '()', '[*]', '...'):
            for command in ('\\c', '@c'):
                with self.subTest(word=word, command=command):
                    source = GOOD.replace('    - fixed a bug\n\n    @section qore_3_0_0',
                                          f'    - the {command} {word} file\n\n    @section qore_3_0_0')
                    issues = checker.inspect_source(source)
                    self.assertEqual(1, len(issues), issues)
                    self.assertIn('renders an empty code span', issues[0][1])
                    self.assertEqual([], checker.inspect_source(source.replace(f'{command} {word}',
                                                                                f'<tt>{word}</tt>')))
        # words that \c renders
        for word in ('*int', '-1', '"x"', '%Qore', 'qore_wrap_dox()'):
            with self.subTest(word=word):
                source = GOOD.replace('    - fixed a bug\n', f'    - fixed \\c {word}\n', 1)
                self.assertEqual([], checker.inspect_source(source))

    def test_entry_length(self):
        def lengthen(text, n):
            return text + ''.join(f'\n      line {i}' for i in range(n))
        # a list item of 4 lines is accepted, one of 5 is not
        for n, expected in ((3, 0), (4, 1)):
            with self.subTest(item_lines=n + 1):
                source = GOOD.replace('    - fixed a bug\n\n    @section qore_3_0_0',
                                      lengthen('    - fixed a bug', n) + '\n\n    @section qore_3_0_0')
                issues = messages(source)
                self.assertEqual(expected, len(issues), issues)
                if expected:
                    self.assertIn('qore_3_1_0: list item of 5 lines; an entry has at most 4 lines', issues[0])
        # nested items are counted separately
        nested = '    - fixed a bug\n' + ''.join(f'      - nested {i}\n' for i in range(8))
        source = GOOD.replace('    - fixed a bug\n\n    @section qore_3_0_0', nested + '\n    @section qore_3_0_0')
        self.assertEqual([], checker.inspect_source(source))
        # a paragraph of a @par entry, also in a @parblock or in the release summary
        para = '\n'.join(f'    sentence {i}' for i in range(5))
        for old, new in (('    One paragraph after a heading without a block.', para),
                         ('    The first paragraph.', para),
                         ('    Major release; highlights:', para)):
            with self.subTest(old=old):
                issues = messages(GOOD.replace(old, para, 1))
                self.assertEqual(1, len(issues), issues)
                self.assertIn('paragraph of 5 lines', issues[0])
        # code blocks are not counted, and older sections are not checked
        code = '    @code{.py}\n' + ''.join(f'x = {i};\n' for i in range(10)) + '    @endcode\n'
        old_code = '    @code{.py}\nstring s = "x";\n\nmy code\n    @endcode\n'
        self.assertIn(old_code, GOOD)
        self.assertEqual([], checker.inspect_source(GOOD.replace(old_code, code, 1)))
        legacy = GOOD.replace('    - a legacy section keeps its historical subsection id',
                              lengthen('    - a legacy section keeps its historical subsection id', 8))
        self.assertEqual([], checker.inspect_source(legacy))

    def test_escape_literal(self):
        for lit in ('\\c "\\r\\n"', '\\c "\\n"', '\\c "1\\r\\nDELE 2"', '@c "a\\tb"'):
            with self.subTest(lit=lit):
                source = GOOD.replace('    - fixed a bug\n', f'    - fixed {lit} handling\n', 1)
                issues = checker.inspect_source(source)
                self.assertEqual(1, len(issues), issues)
                self.assertIn('renders wrongly', issues[0][1])
        # the recommended form and plain \c literals are accepted
        for ok in ('<tt>&quot;\\\\r\\\\n&quot;</tt>', '\\c "abc"', '\\c "%y"'):
            with self.subTest(ok=ok):
                self.assertEqual([], checker.inspect_source(GOOD.replace('    - fixed a bug\n', f'    - fixed {ok}\n', 1)))

    def test_no_sections(self):
        self.assertEqual(['no release sections found'], messages('/** @page release_notes Release Notes */'))

    def test_command_line(self):
        with tempfile.TemporaryDirectory() as directory:
            good = Path(directory) / 'good.dox.tmpl'
            bad = Path(directory) / 'bad.dox.tmpl'
            good.write_text(GOOD, encoding='utf-8')
            bad.write_text(GOOD.replace('    @par Release Summary\n    Bugfix release\n', '    Stray.\n'),
                           encoding='utf-8')
            result = subprocess.run([sys.executable, '-I', str(CHECKER), str(good)], capture_output=True, text=True)
            self.assertEqual(0, result.returncode, result.stderr)
            self.assertIn('0 release-note structure errors', result.stdout)
            result = subprocess.run([sys.executable, '-I', str(CHECKER), str(good), str(bad)], capture_output=True,
                                    text=True)
            self.assertEqual(1, result.returncode)
            self.assertIn(f'{bad}:', result.stderr)
            self.assertNotIn(f'{good}:', result.stderr)
            result = subprocess.run([sys.executable, '-I', str(CHECKER), str(Path(directory) / 'missing')],
                                    capture_output=True, text=True)
            self.assertEqual(1, result.returncode)

    def test_repository_release_notes(self):
        result = subprocess.run([sys.executable, '-I', str(CHECKER)], capture_output=True, text=True)
        self.assertEqual(0, result.returncode, result.stderr)


if __name__ == '__main__':
    unittest.main()
