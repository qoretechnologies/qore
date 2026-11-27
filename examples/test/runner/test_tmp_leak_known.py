# Copyright (C) 2026 Qore Technologies, s.r.o.
"""Checks the list of the tests that still leave files in their temporary directory (qore#7823).

run_tests.sh gives each test a TMPDIR of its own and reports what the test leaves there (QORE_TEST_TMP_LEAKS, by default
"report"); CI sets "fail", which does not fail the tests listed in examples/test/runner/tmp-leak-known.txt.
"""

import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]
KNOWN_FILE = ROOT / "examples" / "test" / "runner" / "tmp-leak-known.txt"


def known_tests():
    return [line.strip() for line in KNOWN_FILE.read_text().splitlines()
            if line.strip() and not line.lstrip().startswith("#")]


class TmpLeakKnownTest(unittest.TestCase):
    def test_entries_are_tests(self):
        for entry in known_tests():
            self.assertFalse(entry.startswith("./") or entry.startswith("/"), entry + ": a path from the root")
            self.assertTrue(entry.endswith(".qtest"), entry + ": a test")
            self.assertTrue((ROOT / entry).is_file(), entry + ": no such test; remove it from the list")

    def test_entries_sorted_and_unique(self):
        entries = known_tests()
        self.assertEqual(sorted(set(entries)), entries, "the list is sorted, with each test once")

    def test_runner_uses_the_list(self):
        runner = (ROOT / "run_tests.sh").read_text()
        self.assertIn('TMP_LEAK_KNOWN_FILE="$BASE_TEST_PATH/runner/tmp-leak-known.txt"', runner)
        self.assertIn('is_known_tmp_leak "$test"', runner)
        # the report is the default, and "off" or an empty value turns it off
        self.assertIn('case "${QORE_TEST_TMP_LEAKS-report}" in', runner)

    def test_ci_fails_leaks(self):
        ci = (ROOT / ".gitlab-ci.yml").read_text()
        self.assertTrue(re.search(r"^  QORE_TEST_TMP_LEAKS: fail$", ci, re.M), "CI fails a test that leaks")


if __name__ == "__main__":
    unittest.main()
