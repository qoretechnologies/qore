#!/usr/bin/env python3
"""Tests of the module API header guard (tools/qore-module-api-headers.py, design/module-api-version.md).

- the source tree: the hash of its installed public headers is the one recorded with its module API in
  cmake/module-api-headers.txt;
- a fixture tree: a change of the code of an installed header (also a header without the .h extension, a string
  that contains a comment marker, a new or a removed header) fails the check until the module API is incremented
  and the headers are recorded again, or the headers are recorded again; a change of comments or whitespace, of an
  internal header or of a header that is not installed does not; a record for another module API fails; a malformed
  record or a tree without the install rule is an error.

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
TOOL = ROOT / "tools/qore-module-api-headers.py"

spec = importlib.util.spec_from_file_location("qore_module_api_headers", TOOL)
guard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guard)

FIXTURE_CMAKE = """install(FILES other.h DESTINATION include)
install(DIRECTORY include/qore
        DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}
        PATTERN intern EXCLUDE
        PATTERN notinstalled.h EXCLUDE)
"""

FIXTURE_HEADER = """// the module API
#define QORE_MODULE_API_MAJOR 2
#define QORE_MODULE_API_MINOR 1
"""


class ModuleApiHeadersTest(unittest.TestCase):
    def run_tool(self, root, *args):
        return subprocess.run([sys.executable, "-I", str(TOOL), "--root", str(root)] + list(args),
                              capture_output=True, text=True)

    def assert_ok(self, root):
        result = self.run_tool(root)
        self.assertEqual((result.returncode, result.stderr), (0, ""))

    def assert_fails(self, root, text):
        result = self.run_tool(root)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn(text, result.stderr)

    def make_tree(self, root):
        (root / "include/qore/intern").mkdir(parents=True)
        (root / "debian").mkdir()
        (root / "cmake").mkdir()
        shutil.copy(ROOT / "debian/module-api.py", root / "debian/module-api.py")
        (root / "CMakeLists.txt").write_text(FIXTURE_CMAKE)
        (root / "include/qore/ModuleManager.h").write_text(FIXTURE_HEADER)
        (root / "include/qore/Value.h").write_text(
            '/* a value */\nclass Value {\n    int v; // the value\n    const char* url = "http://x//y";\n};\n')
        (root / "include/qore/qlist").write_text("template <class T> class qlist { T* head; };\n")
        (root / "include/qore/intern/Private.h").write_text("class Private { int p; };\n")
        (root / "include/qore/notinstalled.h").write_text("class NotInstalled { int n; };\n")
        self.assertEqual(self.run_tool(root, "--update").returncode, 0)
        self.assert_ok(root)

    def edit(self, path, old, new):
        text = path.read_text()
        self.assertEqual(text.count(old), 1)
        path.write_text(text.replace(old, new))

    def test_source_tree_is_recorded(self):
        self.assert_ok(ROOT)
        api, digest = guard.read_record(ROOT)
        self.assertEqual(api, guard._module_api(ROOT))
        self.assertRegex(digest, r"^[0-9a-f]{64}$")
        self.assertEqual(digest, guard.header_hash(ROOT))
        headers = guard.installed_headers(ROOT)
        # the headers without the .h extension are installed and hashed; the internal and excluded ones are not
        for name in ("qlist", "safe_dslist", "vector_map", "vector_set", "ModuleManager.h", "QoreValue.h"):
            self.assertIn(name, headers)
        for name in headers:
            self.assertFalse(name.startswith("intern/"), name)
        for name in ("minitest.hpp", "macros-none.h", "hash_map_include.h", "slist_include.h", "qore-version.h"):
            self.assertNotIn(name, headers)

    def test_neutral_changes_pass(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_tree(root)
            value = root / "include/qore/Value.h"
            self.edit(value, "/* a value */", "/* a value\n   with more text */")
            self.edit(value, "// the value", "//   the stored value")
            self.edit(value, "    int v;", "\tint   v ;")
            (root / "include/qore/intern/Private.h").write_text("class Private { long p; int q; };\n")
            (root / "include/qore/notinstalled.h").write_text("class NotInstalled { long n; };\n")
            self.edit(root / "include/qore/ModuleManager.h", "// the module API", "/** the module API */")
            self.assert_ok(root)

    def test_code_changes_fail_until_recorded(self):
        changes = [
            ("member", lambda root: self.edit(root / "include/qore/Value.h", "int v;", "long v;")),
            ("string with comment marker", lambda root: self.edit(root / "include/qore/Value.h", "//y", "//z")),
            ("header without .h", lambda root: self.edit(root / "include/qore/qlist", "T* head;", "T* head; T* tail;")),
            ("new header", lambda root: (root / "include/qore/New.h").write_text("class New {};\n")),
            ("removed header", lambda root: (root / "include/qore/qlist").unlink()),
            ("renamed header", lambda root: (root / "include/qore/qlist").rename(root / "include/qore/qlist2")),
            ("excluded no more", lambda root: self.edit(root / "CMakeLists.txt", "        PATTERN notinstalled.h EXCLUDE", "")),
        ]
        for name, change in changes:
            with self.subTest(change=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                self.make_tree(root)
                change(root)
                self.assert_fails(root, "the public headers changed without a change of the module API 2.1")
                self.assertEqual(self.run_tool(root, "--update").returncode, 0)
                self.assert_ok(root)

    def test_api_change_requires_a_new_record(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_tree(root)
            self.edit(root / "include/qore/Value.h", "int v;", "long v;")
            self.edit(root / "include/qore/ModuleManager.h", "MINOR 1", "MINOR 2")
            self.assert_fails(root, "the module API is 2.2, but cmake/module-api-headers.txt was recorded for "
                              "module API 2.1")
            self.assertEqual(self.run_tool(root, "--update").returncode, 0)
            self.assertEqual(guard.read_record(root)[0], "2.2")
            self.assert_ok(root)

    def test_errors(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_tree(root)
            record = root / guard.RECORD
            good = record.read_text()
            for text in ("", "api 2.1\n", "sha256 x\n", good + "extra 1\n"):
                with self.subTest(record=text):
                    record.write_text(text)
                    result = self.run_tool(root)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("must hold exactly the 'api' and 'sha256' lines", result.stderr)
            record.write_text(good)
            (root / "CMakeLists.txt").write_text("install(FILES x.h DESTINATION include)\n")
            result = self.run_tool(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("no install(DIRECTORY include/qore ...) rule", result.stderr)

    @unittest.skipUnless(shutil.which("git"), "requires git")
    def test_git_checkout_hashes_tracked_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_tree(root)
            # the same tree as a git checkout: the tracked files are the files of the tree, so the record holds
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            subprocess.run(["git", "-C", str(root), "add", "-A"], check=True)
            self.assertIsNotNone(guard._git_files(root))
            self.assert_ok(root)
            # untracked files are not sources: editor and merge leftovers do not count
            for name in (".DS_Store", "Value.h.orig", "Value.h.rej", ".Value.h.swp", "intern/.DS_Store"):
                (root / "include/qore" / name).write_text("junk\n")
            self.assert_ok(root)
            # a header that is added to git counts
            (root / "include/qore/New.h").write_text("class New {};\n")
            self.assert_ok(root)
            subprocess.run(["git", "-C", str(root), "add", "include/qore/New.h"], check=True)
            self.assert_fails(root, "the public headers changed without a change of the module API 2.1")
            subprocess.run(["git", "-C", str(root), "rm", "-q", "--cached", "include/qore/New.h"], check=True)
            self.assert_ok(root)
            # a tracked header deleted in the working tree is not installed
            (root / "include/qore/qlist").unlink()
            self.assert_fails(root, "the public headers changed without a change of the module API 2.1")

    def test_tarball_hashes_all_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_tree(root)
            self.assertIsNone(guard._git_files(root))
            (root / "include/qore/.DS_Store").write_text("junk\n")
            self.assert_fails(root, "the public headers changed without a change of the module API 2.1")

    def test_normalize(self):
        self.assertEqual(guard.normalize("a // c\n/* d\n e */ b"), "a b")
        self.assertEqual(guard.normalize("a/* c */b"), "a b")
        self.assertEqual(guard.normalize('s = "a // b /* c */";'), 's="a // b /* c */";')
        self.assertEqual(guard.normalize("c = '/'; d = '\\''; // x"), "c='/';d='\\'';")
        self.assertEqual(guard.normalize("unsigned\t\n  int x ( int y ) ;"), "unsigned int x(int y);")
        self.assertEqual(guard.normalize("return \"a  b\" ;"), 'return"a  b";')
        self.assertNotEqual(guard.normalize("a b"), guard.normalize("ab"))


if __name__ == "__main__":
    unittest.main()
