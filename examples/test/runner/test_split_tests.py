#!/usr/bin/env python3
"""A long test can be split into parts: its class, in <name>.qc, adds each case with addPartCase(<part>, ...), and each
<name>-<part>.qtest runs the cases of one part, so that no test takes long (CMakeBuildHelpers and
AOTSccIncrementalDriver, which took 224 s and 165 s in the macOS CI job).  These tests fail when a case is in a part that
no test runs, or a part test runs no case, so that the parts together always run every case.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[3]
PART_CASE = re.compile(r"\baddPartCase\(\s*(\d+)\s*,")


class SplitTestsTest(unittest.TestCase):
    def split_classes(self):
        return [path for path in sorted((ROOT / "examples/test").rglob("*.qc"))
                if PART_CASE.search(path.read_text(errors="replace"))]

    def test_every_part_runs(self):
        classes = self.split_classes()
        self.assertGreaterEqual(len(classes), 2)
        for qc in classes:
            with self.subTest(qc=str(qc.relative_to(ROOT))):
                name = qc.stem
                parts = sorted({int(n) for n in PART_CASE.findall(qc.read_text())})
                tests = sorted(qc.parent.glob(name + "-*.qtest"))
                self.assertEqual(["%s-%d.qtest" % (name, n) for n in range(1, len(tests) + 1)],
                                 [t.name for t in tests], "the part tests are numbered from 1")
                self.assertEqual(list(range(1, len(tests) + 1)), parts,
                                 "each part test runs cases, and each case is in a part that a test runs")
                for n, test in enumerate(tests, 1):
                    text = test.read_text()
                    self.assertRegex(text, r"(?m)^%%include %s\.qc$" % re.escape(name))
                    self.assertRegex(text, r'%sBase\("[^"]+", %d\)' % (re.escape(name), n),
                                     "%s runs part %d" % (test.name, n))
                    self.assertTrue(test.stat().st_mode & 0o111, "%s is executable" % test.name)
                # the class itself is not a test
                self.assertFalse((qc.parent / (name + ".qtest")).exists())

    def test_part_helper_adds_only_its_part(self):
        for qc in self.split_classes():
            text = qc.read_text()
            with self.subTest(qc=qc.name):
                self.assertRegex(text, r"(?s)private addPartCase\(int case_part, string name, code call, "
                                       r"\*softlist<auto> args\) \{\s*if \(case_part == part\) \{\s*"
                                       r"addTestCase\(name, call, args\);")
                # every case of the constructor goes through it
                ctor = text.split("    constructor(string name, int part)", 1)[1].split("set_return_value(main())")[0]
                self.assertNotIn("addTestCase(", ctor)


if __name__ == "__main__":
    unittest.main()
