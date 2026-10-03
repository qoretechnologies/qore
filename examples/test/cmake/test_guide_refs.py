#!/usr/bin/env python3
"""Regression tests for references to extracted module guides.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


sys.dont_write_bytecode = True
CHECKER = Path(__file__).resolve().parents[3] / "doxygen/check-guide-refs.py"
SPEC = importlib.util.spec_from_file_location("guide_refs", CHECKER)
REFS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REFS)


class GuideReferencesTest(unittest.TestCase):
    def test_old_bookmark_link_reports_source_line_and_destination(self):
        sources = {"main": "@anchor mysqlerrorinfo @subpage mysqlerrorinfoguide",
                   "guide": "@page mysqlerrorinfoguide Error Information",
                   "notes": "Release notes\nSee @ref mysqlerrorinfo\n"}
        self.assertEqual([("notes", 2, "mysqlerrorinfo", "mysqlerrorinfoguide")], REFS.check_sources(sources))

    def test_direct_guide_links_and_bookmarks_are_preserved(self):
        self.assertEqual([], REFS.check_sources({
            "main": "@anchor old @subpage guide\n@anchor intro",
            "guide": '@page guide The Guide\n@ref intro "Introduction"\n@ref guide "Details"'}))

    def test_backslashes_multiple_anchors_multiline_links_and_labels(self):
        self.assertEqual([("api", 1, "first", "guide"), ("api", 2, "second", "guide")],
                         REFS.check_sources({"main": "\\anchor first\n@anchor second\n\\ref guide",
                                             "guide": "\\page guide Guide",
                                             "api": '\\ref first "Details"\n@subpage second'}))

    def test_class_links_and_longer_names_are_not_guide_aliases(self):
        self.assertEqual([], REFS.check_sources({
            "main": "@anchor protocol @ref SmtpClient\n@anchor old @subpage guide",
            "guide": "@page guide Guide\n@ref protocol\n@ref old_extra\n@ref old::method"}))

    def test_see_also_commands_reject_old_bookmarks(self):
        self.assertEqual([("api.java", 1, "old", "guide"), ("api.java", 2, "old", "guide")],
                         REFS.check_sources({"main": "@anchor old @subpage guide",
                                             "guide": "@page guide Guide",
                                             "api.java": "@see old\n\\sa old\n@see @ref guide"}))

    def test_sentence_punctuation_does_not_hide_stale_links(self):
        self.assertEqual([("guide", 2, "old", "guide"), ("guide", 3, "old", "guide")],
                         REFS.check_sources({"main": "@anchor old @subpage guide",
                                             "guide": "@page guide Guide\nSee @ref old.\n(@ref old)."}))

    def test_literal_examples_are_ignored_without_changing_line_numbers(self):
        self.assertEqual([("guide", 7, "old", "guide")], REFS.check_sources({
            "main": "@anchor old @ref guide",
            "guide": "@page guide Guide\n@code{.py}\n@ref old\n@endcode\n"
                     "\\verbatim @ref old\n\\endverbatim\n@ref old"}))

    def test_command_line_checks_tracked_sources_and_ignores_build_output(self):
        with tempfile.TemporaryDirectory(prefix="guide-refs-test-") as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            docs = root / "docs"
            docs.mkdir()
            (docs / "main.dox.tmpl").write_text("@anchor old @subpage guide\n")
            guide = docs / "guide.dox.tmpl"
            guide.write_text("@page guide Guide\n@ref old\n")
            (root / "src/java").mkdir(parents=True)
            java = root / "src/java/Api.java"
            java.write_text("/** @see old */\nclass Api {}\n")
            subprocess.run(["git", "-C", str(root), "add", "docs", "src"], check=True)
            command = [sys.executable, str(CHECKER), str(root)]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(1, result.returncode)
            self.assertIn("guide.dox.tmpl:2:", result.stderr)
            self.assertIn("Api.java:1:", result.stderr)
            self.assertIn("use @ref guide", result.stderr)
            guide.write_text("@page guide Guide\n@ref guide\n")
            java.write_text("/** @see @ref guide */\nclass Api {}\n")
            (docs / "generated.dox").write_text("@ref old\n")
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(0, result.returncode, result.stderr)

    def test_invalid_or_empty_repository_is_an_error(self):
        with tempfile.TemporaryDirectory(prefix="guide-refs-empty-") as directory:
            for initialized in (False, True):
                if initialized:
                    subprocess.run(["git", "init", "-q", directory], check=True)
                result = subprocess.run([sys.executable, str(CHECKER), directory], capture_output=True, text=True)
                self.assertEqual(2, result.returncode)
                self.assertIn("error:", result.stderr)


if __name__ == "__main__":
    unittest.main()
