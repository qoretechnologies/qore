#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Exercise build-time source identity with real Git repositories and archives."""
import io
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")


class GitRevisionTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qore-source-revision-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def command(self, *args, cwd=None, success=True):
        result = subprocess.run(list(map(str, args)), cwd=cwd, capture_output=True, text=True, timeout=60)
        if success:
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertNotRegex(result.stdout + result.stderr, r"(?im)\bwarning\b|^fatal:")
        else:
            self.assertNotEqual(0, result.returncode, result.stdout + result.stderr)
        return result

    def git(self, source, *args):
        return self.command("git", "-c", "user.name=Qore build test", "-c", "user.email=build-test@invalid",
                            "-c", "commit.gpgsign=false", "-c", "core.hooksPath=/dev/null", *args,
                            cwd=source).stdout.strip()

    def source(self, name, git=True):
        source = self.root / name
        (source / "cmake").mkdir(parents=True)
        for name in (".gitattributes", ".git-archive-revision", "cmake/UpdateGitRevision.cmake",
                     "cmake/QoreMacrosIntern.cmake"):
            shutil.copyfile(ROOT / name, source / name)
        (source / "CMakeLists.txt").write_text('cmake_minimum_required(VERSION 3.14)\n'
            'project(SourceRevision NONE)\ninclude(cmake/QoreMacrosIntern.cmake)\ncreate_git_revision()\n')
        if git:
            self.git(source, "init", "-q", "-b", "main")
            self.git(source, "add", ".")
            self.git(source, "commit", "-q", "-m", "fixture")
        return source

    def update(self, source, output=None, success=True, *extra):
        output = output or source / "result with spaces.h"
        result = self.command(CMAKE, f"-DSOURCE_DIR={source}", f"-DOUTPUT_FILE={output}", *extra,
                              "-P", ROOT / "cmake/UpdateGitRevision.cmake", success=success)
        return output, result

    def header(self, output, revision):
        self.assertEqual(f'#define BUILD "{revision}"\n', output.read_text())

    def test_configure_and_build_follow_head_without_rewriting_unchanged_header(self):
        source = self.source("checkout [with]+ spaces")
        build = self.root / "build with spaces"
        self.command(CMAKE, "-S", source, "-B", build)
        output = build / "include/qore/intern/git-revision.h"
        self.header(output, self.git(source, "rev-parse", "HEAD"))
        os.utime(output, ns=(1_000_000_000, 1_000_000_000))
        self.command(CMAKE, "--build", build)
        self.assertEqual(1_000_000_000, output.stat().st_mtime_ns)
        (source / "changed.txt").write_text("next source revision\n")
        self.git(source, "add", "changed.txt")
        self.git(source, "commit", "-q", "-m", "next")
        self.command(CMAKE, "--build", build)
        self.header(output, self.git(source, "rev-parse", "HEAD"))

    def test_linked_worktree_uses_its_own_head(self):
        source = self.source("repository")
        original = self.git(source, "rev-parse", "HEAD")
        linked = self.root / "linked worktree"
        self.git(source, "worktree", "add", "--quiet", "--detach", linked, original)
        self.assertTrue((linked / ".git").is_file())
        (source / "new.txt").write_text("advance original\n")
        self.git(source, "add", "new.txt")
        self.git(source, "commit", "-q", "-m", "new")
        output, _ = self.update(linked)
        self.header(output, original)

    def test_real_archive_keeps_revision_inside_unrelated_repository_without_git(self):
        source = self.source("exported repository")
        expected = self.git(source, "rev-parse", "HEAD")
        outer = self.source("unrelated repository")
        (outer / "unrelated.txt").write_text("different parent repository\n")
        self.git(outer, "add", "unrelated.txt")
        self.git(outer, "commit", "-q", "-m", "unrelated parent")
        self.assertNotEqual(expected, self.git(outer, "rev-parse", "HEAD"))
        archive = subprocess.check_output(["git", "archive", "HEAD"], cwd=source)
        extracted = outer / "archive"
        with tarfile.open(fileobj=io.BytesIO(archive)) as bundle:
            bundle.extractall(extracted, filter="data")
        self.assertIn(expected, (extracted / ".git-archive-revision").read_text())
        output, _ = self.update(extracted, None, True, "-DCMAKE_DISABLE_FIND_PACKAGE_Git=TRUE")
        self.header(output, expected)
        build = self.root / "archive build"
        self.command(CMAKE, "-S", extracted, "-B", build)
        self.command(CMAKE, "--build", build)
        self.header(build / "include/qore/intern/git-revision.h", expected)

    def test_unversioned_copy_does_not_borrow_parent_head_or_keep_stale_identity(self):
        outer = self.source("parent")
        source = self.source("parent/unversioned", git=False)
        output = source / "old.h"
        for marker in (True, False):
            if not marker:
                (source / ".git-archive-revision").unlink()
            output.write_text('#define BUILD "stale"\n')
            self.update(source, output)
            self.header(output, "unknown")
        self.assertNotEqual("unknown", self.git(outer, "rev-parse", "HEAD"))

    def test_invalid_archive_identity_does_not_replace_output(self):
        source = self.source("invalid", git=False)
        output = source / "previous.h"
        for marker in ("", "# comments only\n", "abc\n", "unknown\n", "0" * 39 + "\n",
                       "f" * 41 + "\n", "a" * 40 + "\n" + "b" * 40 + "\n", 'bad"revision\n'):
            with self.subTest(marker=marker):
                (source / ".git-archive-revision").write_text(marker)
                output.write_text("unchanged\n")
                _, result = self.update(source, output, success=False)
                self.assertIn("Invalid", result.stderr)
                self.assertEqual("unchanged\n", output.read_text())
        for length in (40, 64):
            revision = "a" * length
            (source / ".git-archive-revision").write_text(revision + "\n")
            self.update(source, output)
            self.header(output, revision)

    def test_broken_worktree_and_missing_git_do_not_fall_back_to_archive(self):
        source = self.source("broken")
        output, _ = self.update(source)
        original = output.read_bytes()
        _, result = self.update(source, output, False, "-DCMAKE_DISABLE_FIND_PACKAGE_Git=TRUE")
        self.assertIn("Git is needed", result.stderr)
        self.assertEqual(original, output.read_bytes())
        (source / ".git/HEAD").write_text("ref: refs/heads/missing\n")
        _, result = self.update(source, output, success=False)
        self.assertIn("Cannot read the working tree revision", result.stderr)
        self.assertEqual(original, output.read_bytes())

    def test_missing_arguments_and_nonexistent_source_fail(self):
        for args in ([], ["-DSOURCE_DIR="], [f"-DSOURCE_DIR={self.root}", "-DOUTPUT_FILE="],
                     [f"-DSOURCE_DIR={self.root}/absent", f"-DOUTPUT_FILE={self.root}/out.h"]):
            with self.subTest(args=args):
                self.command(CMAKE, *args, "-P", ROOT / "cmake/UpdateGitRevision.cmake", success=False)
        self.assertFalse((self.root / "out.h").exists())


if __name__ == "__main__":
    unittest.main()
