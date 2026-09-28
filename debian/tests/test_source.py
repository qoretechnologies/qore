#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Check snapshot changelog ordering with Debian's actual changelog parser."""

import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("source", ROOT / "tools/prepare-debian-source.py")
source = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(source)
PENDING = """qore (3.0.0-1) unstable; urgency=medium

  * Package the bundled YAML module.

 -- David Nichols <david@qore.org>  Fri, 06 Feb 2026 00:00:00 +0000
"""
HISTORY = """
qore (2.2.1-1) unstable; urgency=low

  * Previous release.

 -- David Nichols <david@qore.org>  Thu, 05 Feb 2026 00:00:00 +0000
"""
VERSION = "3.0.0~git20260928.3-0qore1~ubuntu26.04"


class SourcePreparationTest(unittest.TestCase):
    def prepare(self, text):
        return source.snapshot_changelog(text, VERSION, "resolute", "abc123",
            "Mon, 28 Sep 2026 00:00:00 +0000", "David Nichols <david@qore.org>")

    def test_pending_entry_is_versioned_and_history_preserved(self):
        result = self.prepare(PENDING + HISTORY)
        self.assertNotIn("qore (3.0.0-1)", result)
        self.assertIn("Package the bundled YAML module.", result)
        self.assertIn("Testing snapshot from commit abc123.", result)
        self.assertTrue(result.endswith(HISTORY))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "changelog"
            path.write_text(result)
            parsed = subprocess.run(["dpkg-parsechangelog", "-l", str(path), "--all"],
                                    text=True, capture_output=True, check=True)
            self.assertEqual("", parsed.stderr)
            self.assertIn("Version: " + VERSION, parsed.stdout)
            self.assertIn("Distribution: resolute", parsed.stdout)

    def test_single_pending_entry_has_no_future_history(self):
        result = self.prepare(PENDING)
        self.assertEqual(1, result.count("\n -- "))
        self.assertEqual(result, self.prepare(PENDING))

    def test_cannot_precede_historical_release(self):
        with self.assertRaisesRegex(ValueError, "must follow historical version"):
            self.prepare(PENDING + HISTORY.replace("2.2.1-1", "3.0.0-1"))

    def test_incomplete_entry_is_rejected(self):
        with self.assertRaises(ValueError):
            self.prepare("qore (3.0.0-1) unstable; urgency=medium\n")


if __name__ == "__main__":
    unittest.main()
