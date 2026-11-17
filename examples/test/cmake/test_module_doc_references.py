#!/usr/bin/env python3
"""Tests doxygen/check-module-doc-references.py, the check of a bundled module's final documentation pass for
references to modules outside its QORE_DOC_REFERENCE_MODULES_<module> list (issue #6367).

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "doxygen" / "check-module-doc-references.py"

spec = importlib.util.spec_from_file_location("check_module_doc_references", SCRIPT)
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)

TAG = """<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile doxygen_version="1.9.8">
  <compound kind="namespace">
    <name>NsA</name>
    <filename>namespaceNsA.html</filename>
    <class kind="class">NsA::Foo</class>
  </compound>
  <compound kind="class">
    <name>NsA::Foo</name>
    <filename>classNsA_1_1Foo.html</filename>
    <member kind="function">
      <type>void</type>
      <name>bar</name>
      <anchorfile>classNsA_1_1Foo.html</anchorfile>
      <anchor>a1</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="page">
    <name>a_page</name>
    <title>A page</title>
    <filename>a_page.html</filename>
    <docanchor file="a_page.html" title="A section">a_sect</docanchor>
  </compound>
</tagfile>
"""


def warning(target, explicit=False, link=False):
    if link:
        return f"/src/b.dox:2: warning: unable to resolve link to '{target}' for \\link command"
    if explicit:
        return f"/src/b.dox:2: warning: explicit link request to '{target}' could not be resolved"
    return f"/src/b.dox:2: warning: unable to resolve reference to '{target}' for \\ref command"


class ModuleDocReferencesTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="qore doc refs ")
        self.addCleanup(directory.cleanup)
        self.dir = Path(directory.name)
        self.tag = self.dir / "A.tag"
        self.tag.write_text(TAG)

    def run_script(self, log, *args):
        path = self.dir / "warnings.log"
        if log is not None:
            path.write_text(log)
        return subprocess.run([sys.executable, "-I", "-W", "error", str(SCRIPT), "--module", "B", "--log", str(path),
                               "--list-variable", "QORE_DOC_REFERENCE_MODULES_B", *args],
                              text=True, capture_output=True, timeout=60)

    def test_tag_names(self):
        names = checker.tag_names(self.tag)
        for name in ("NsA", "NsA::Foo", "NsA::Foo::bar", "a_page", "a_sect"):
            self.assertIn(name, names)
        # a member is not known by its unqualified name, which any module can use
        self.assertNotIn("bar", names)

    def test_normalize(self):
        self.assertEqual("NsA::Foo::bar", checker.normalize("NsA::Foo::bar(int x)"))
        self.assertEqual("NsA::Foo::bar", checker.normalize("::NsA::Foo::bar()"))
        self.assertEqual("NsA::Foo::bar", checker.normalize("NsA::Foo#bar"))
        self.assertEqual("a_page", checker.normalize("a_page"))
        self.assertEqual("NsA::Foo::bar", checker.normalize("NsA::Foo::bar() const"))

    def test_reference_to_an_unlisted_module_fails(self):
        for target, explicit in (("a_page", False), ("a_sect", False), ("NsA::Foo", False),
                                 ("NsA::Foo::bar()", False), ("NsA::Foo::bar()", True)):
            with self.subTest(target=target, explicit=explicit):
                result = self.run_script(warning(target, explicit) + "\n", "--tag", f"A={self.tag}")
                self.assertEqual(1, result.returncode, result.stderr)
                self.assertIn("QORE_DOC_REFERENCE_MODULES_B", result.stderr)
                self.assertIn("of module A", result.stderr)
                # the warning itself is shown, as the log replaces the output of Doxygen
                self.assertIn(warning(target, explicit), result.stderr)

    def test_link_command_to_an_unlisted_module_fails(self):
        result = self.run_script(warning("NsA::Foo", link=True) + "\n", "--tag", f"A={self.tag}")
        self.assertEqual(1, result.returncode, result.stderr)
        self.assertIn("refers to 'NsA::Foo' of module A", result.stderr)

    def doxygen_command(self, warning_text, rc):
        """a stand-in for Doxygen that writes the log given and exits with rc"""
        script = self.dir / "fake-doxygen.py"
        script.write_text(f"from pathlib import Path\nimport sys\n"
                          f"Path({str(self.dir / 'warnings.log')!r}).write_text({warning_text!r})\nsys.exit({rc})\n")
        return ["--", sys.executable, "-I", str(script)]

    def test_runs_doxygen_and_checks_its_warnings(self):
        result = self.run_script(None, "--tag", f"A={self.tag}", *self.doxygen_command(warning("a_page") + "\n", 0))
        self.assertEqual(1, result.returncode, result.stderr)
        self.assertIn("refers to 'a_page' of module A", result.stderr)
        result = self.run_script(None, "--allowed", "A", "--tag", f"A={self.tag}",
                                 *self.doxygen_command(warning("a_page") + "\n", 0))
        self.assertEqual(0, result.returncode, result.stderr)

    def test_failed_doxygen_shows_its_errors(self):
        # with WARN_LOGFILE, Doxygen writes its errors only to the log: they are printed, and its exit code is kept
        error = "error: tag OUTPUT_DIRECTORY: Output directory '/x' does not exist and cannot be created"
        result = self.run_script(None, "--allowed", "A", "--tag", f"A={self.tag}",
                                 *self.doxygen_command(error + "\n", 3))
        self.assertEqual(3, result.returncode, result.stderr)
        self.assertIn(error, result.stderr)
        self.assertIn("failed with exit code 3", result.stderr)

    def test_log_of_an_earlier_run_is_not_checked(self):
        # a run that writes no log (no warnings) is not reported with the warnings of the run before it
        script = self.dir / "quiet-doxygen.py"
        script.write_text("")
        result = self.run_script(warning("a_page") + "\n", "--tag", f"A={self.tag}",
                                 "--", sys.executable, "-I", str(script))
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertNotIn("a_page", result.stderr)

    def test_reference_to_a_listed_module_passes(self):
        result = self.run_script(warning("a_page") + "\n", "--allowed", "A", "--tag", f"A={self.tag}")
        self.assertEqual(0, result.returncode, result.stderr)
        # the unresolved reference is still reported as the warning that it is
        self.assertIn(warning("a_page"), result.stderr)
        self.assertNotIn("error:", result.stderr)

    def test_reference_that_no_module_defines_passes(self):
        # a typo or a missing symbol is a Doxygen warning, not a missing import
        result = self.run_script(warning("no_such_page") + "\n" + warning("bar()") + "\n", "--tag", f"A={self.tag}")
        self.assertEqual(0, result.returncode, result.stderr)

    def test_reference_that_the_module_itself_defines_passes(self):
        # a name that the module's own documentation defines is not a reference to another module
        result = self.run_script(warning("a_page") + "\n", "--tag", f"A={self.tag}", "--tag", f"B={self.tag}")
        self.assertEqual(0, result.returncode, result.stderr)

    def test_no_log_and_missing_tag_file_pass(self):
        # a pass without warnings writes an empty log or none; a module whose documentation is not built has no tag
        self.assertEqual(0, self.run_script(None, "--tag", f"A={self.tag}").returncode)
        self.assertEqual(0, self.run_script("", "--tag", f"A={self.tag}").returncode)
        result = self.run_script(warning("a_page") + "\n", "--tag", f"A={self.dir / 'missing.tag'}")
        self.assertEqual(0, result.returncode, result.stderr)

    def test_invalid_tag_argument_is_an_error(self):
        result = self.run_script("", "--tag", "A")
        self.assertEqual(2, result.returncode)
        self.assertIn("MODULE=TAGFILE", result.stderr)

    def test_other_warnings_are_ignored(self):
        log = "/src/b.dox:5: warning: Found unknown command '\\foo'\n"
        result = self.run_script(log, "--tag", f"A={self.tag}")
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertIn("unknown command", result.stderr)

    @unittest.skipUnless(shutil.which("doxygen"), "doxygen is not available")
    def test_doxygen_warnings(self):
        # the warnings of the Doxygen in use, for a module that refers to another without importing its tag file
        (self.dir / "a").mkdir()
        (self.dir / "b").mkdir()
        (self.dir / "a" / "a.dox").write_text("/** @mainpage A\n    text\n*/\n"
                                             "/** @page a_page A page\n    @section a_sect A section\n    text\n*/\n")
        (self.dir / "a" / "a.h").write_text("namespace NsA {\n/** a class */\nclass Foo {\npublic:\n"
                                           "    /** a method */\n    void bar();\n};\n}\n")
        (self.dir / "b" / "b.dox").write_text("/** @mainpage B\n    see @ref a_page, @ref a_sect, @ref NsA::Foo and "
                                             "@ref NsA::Foo::bar()\n*/\n")
        (self.dir / "Doxyfile.a").write_text("PROJECT_NAME=A\nINPUT=a\nOUTPUT_DIRECTORY=out-a\n"
                                             "GENERATE_TAGFILE=a-real.tag\nGENERATE_LATEX=NO\nQUIET=YES\n")
        (self.dir / "Doxyfile.b").write_text("PROJECT_NAME=B\nINPUT=b\nOUTPUT_DIRECTORY=out-b\nGENERATE_LATEX=NO\n"
                                             "QUIET=YES\nWARN_IF_DOC_ERROR=YES\nWARN_LOGFILE=b.log\n")
        for doxyfile in ("Doxyfile.a", "Doxyfile.b"):
            subprocess.run(["doxygen", doxyfile], cwd=self.dir, capture_output=True, timeout=120, check=True)
        log = (self.dir / "b.log").read_text()
        tag = self.dir / "a-real.tag"
        result = self.run_script(log, "--tag", f"A={tag}")
        self.assertEqual(1, result.returncode, result.stderr)
        for target in ("a_page", "a_sect", "NsA::Foo", "NsA::Foo::bar"):
            self.assertIn(f"refers to '{target}' of module A", result.stderr)
        self.assertEqual(0, self.run_script(log, "--allowed", "A", "--tag", f"A={tag}").returncode)


if __name__ == "__main__":
    unittest.main()
