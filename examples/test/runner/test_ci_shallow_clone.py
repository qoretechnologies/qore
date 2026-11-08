#!/usr/bin/env python3
"""The CI jobs clone a shallow history (GIT_DEPTH in .gitlab-ci.yml), so nothing the CI jobs build or run may read
more of the history than HEAD and its parents.

The build embeds the HEAD commit (cmake/UpdateGitRevision.cmake), the checks list the tracked files (git ls-files)
and tree-already-tested.sh compares the trees of HEAD and its parents.  These tests fail when .gitlab-ci.yml stops
cloning shallow, or when a build or CI file starts to run a git command that needs history (git describe, git log, a
diff against another commit, ...), which a shallow clone does not have; and they build the revision header from a
shallow clone.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]

# The git commands that work on a clone of depth 3 (HEAD, its parent and its grandparent): rev-parse of HEAD, of its
# tree and of its parents' trees, ls-files, and rev-list --parents -n 1 HEAD (the parents of HEAD)
SHALLOW_SAFE = {"rev-parse", "ls-files", "rev-list --parents -n 1 HEAD"}

# The files that the CI jobs run to build Qore and to run its checks and tests (the tests in examples/test that use
# git make fixture repositories of their own)
def ci_files():
    files = [ROOT / "CMakeLists.txt", ROOT / "run_tests.sh", ROOT / "getrev.sh", ROOT / ".gitlab-ci.yml"]
    files += sorted((ROOT / "cmake").rglob("*.cmake"))
    files += sorted((ROOT / "examples/test/docker_test").glob("*.sh"))
    files += sorted((ROOT / "doxygen").glob("*.py"))
    return files


# the git commands (words after "git" that are none of these, as in a message, are not commands)
GIT_COMMANDS = ("add am apply archive bisect blame branch bundle cat-file checkout cherry cherry-pick clean clone commit "
                "config count-objects describe diff diff-index diff-tree fetch for-each-ref format-patch gc grep init "
                "log ls-files ls-remote ls-tree merge merge-base mv name-rev notes pull push range-diff rebase reflog "
                "remote reset restore rev-list rev-parse revert rm shortlog show show-branch show-ref sparse-checkout "
                "stash status submodule switch tag whatchanged worktree").split()

# a git command in code: "git [-C dir] <command> ...", "${GIT_EXECUTABLE}" <command>, or ["git", ["-C", dir,] "<command>"
GIT_COMMAND = re.compile(r'(?:(?<![\w./-])git(?:\s+-C\s+(?:"[^"]*"|\S+))?\s+|\$\{GIT_EXECUTABLE\}"?\s+)'
                         r'(?P<command>[a-z][a-z-]*(?:\s+--parents\s+-n\s+1\s+HEAD)?)'
                         r'|\["git",\s*(?:"-C",\s*[^,]+,\s*)?"(?P<pycommand>[a-z][a-z-]*)"')


def code_lines(path):
    """Yields the lines of a file that are not comments, with their numbers"""
    for number, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
        if line.lstrip().startswith("#"):
            continue
        yield number, line


class ShallowCloneTest(unittest.TestCase):
    def test_jobs_clone_shallow(self):
        # the depth is set for every job, in the global variables, and no job sets a depth of its own
        text = (ROOT / ".gitlab-ci.yml").read_text()
        top = re.search(r"(?m)^variables:\n((?:[ \t]+.*\n|[ \t]*\n)+)", text)
        self.assertIsNotNone(top, "the global variables of .gitlab-ci.yml")
        depth = re.search(r'(?m)^  GIT_DEPTH:\s*"?(\d+)"?\s*$', top.group(1))
        self.assertIsNotNone(depth, "GIT_DEPTH is set in the global variables")
        # 3 commits: HEAD and its parents, whose trees tree-already-tested.sh compares, and a margin
        self.assertGreaterEqual(int(depth.group(1)), 2)
        self.assertLessEqual(int(depth.group(1)), 10, "a shallow clone")
        self.assertEqual(1, len(re.findall(r"(?m)^\s+GIT_DEPTH:", text)), "no job sets a depth of its own")
        # a full clone of the history would be fetched again by these
        self.assertNotRegex(text, r"GIT_STRATEGY:\s*clone|--unshallow|fetch\s+--depth")

    def test_ci_files_need_no_history(self):
        found = []
        for path in ci_files():
            for number, line in code_lines(path):
                for match in GIT_COMMAND.finditer(line):
                    command = match.group("command") or match.group("pycommand")
                    if command.split()[0] not in GIT_COMMANDS:
                        continue
                    found.append(command)
                    if command not in SHALLOW_SAFE:
                        self.fail("%s:%d runs 'git %s', which needs more history than a shallow clone has: %s"
                                  % (path.relative_to(ROOT), number, command, line.strip()))
        # the scan sees the commands the build and the checks run
        self.assertIn("rev-parse", found)
        self.assertIn("ls-files", found)
        self.assertIn("rev-list --parents -n 1 HEAD", found)

    def test_scan_finds_history_commands(self):
        # the pattern finds the commands that need history in each form the files use
        for line, command in (('git describe --tags', "describe"), ('git -C "${QORE_SRC_DIR}" log -1', "log"),
                              ('COMMAND "${GIT_EXECUTABLE}" merge-base HEAD origin/develop', "merge-base"),
                              ('subprocess.run(["git", "-C", str(root), "diff", "HEAD~5"])', "diff"),
                              ('subprocess.run(["git", "rev-list", "--count", "HEAD"])', "rev-list")):
            with self.subTest(line=line):
                match = GIT_COMMAND.search(line)
                self.assertIsNotNone(match)
                self.assertEqual(command, match.group("command") or match.group("pycommand"))
        # git as part of another word or path is not a command
        self.assertNotIn(GIT_COMMAND.search('echo "-- git requests are authenticated --"').group("command"),
                         GIT_COMMANDS)
        for line in ("set(_git_rev_file x)", "qlib/GitHubDataProvider", ".git-archive-revision file", "gitdir: x"):
            with self.subTest(line=line):
                self.assertIsNone(GIT_COMMAND.search(line))

    @unittest.skipUnless(shutil.which("git") and shutil.which("cmake"), "needs git and cmake")
    def test_revision_from_shallow_clone(self):
        # the build reads the revision of HEAD from a clone of depth 3 of a longer history with a merge
        with tempfile.TemporaryDirectory(prefix="qore-shallow-clone-") as directory:
            origin = Path(directory) / "origin"
            origin.mkdir()

            def git(cwd, *args):
                return subprocess.run(["git", "-c", "user.name=Qore CI test", "-c", "user.email=ci-test@invalid",
                                       "-c", "init.defaultBranch=main", *args], cwd=cwd, check=True,
                                      capture_output=True, text=True).stdout.strip()

            git(origin, "init", "-q")
            for n in range(6):
                (origin / "file").write_text("%d\n" % n)
                git(origin, "add", "file")
                git(origin, "commit", "-q", "-m", "commit %d" % n)
            git(origin, "checkout", "-q", "-b", "topic")
            (origin / "topic").write_text("topic\n")
            git(origin, "add", "topic")
            git(origin, "commit", "-q", "-m", "topic")
            git(origin, "checkout", "-q", "main")
            git(origin, "merge", "-q", "--no-ff", "-m", "merge", "topic")
            head = git(origin, "rev-parse", "HEAD")

            clone = Path(directory) / "clone"
            git(directory, "clone", "-q", "--depth", "3", "file://%s" % origin, str(clone))
            self.assertEqual("true", git(clone, "rev-parse", "--is-shallow-repository"))
            output = Path(directory) / "git-revision.h"
            subprocess.run(["cmake", "-DSOURCE_DIR=%s" % clone, "-DOUTPUT_FILE=%s" % output, "-P",
                            str(ROOT / "cmake/UpdateGitRevision.cmake")], check=True, capture_output=True)
            self.assertIn('#define BUILD "%s"' % head, output.read_text())
            # the parents of HEAD and their trees, which tree-already-tested.sh compares with the tree of HEAD
            parents = git(clone, "rev-list", "--parents", "-n", "1", "HEAD").split()[1:]
            self.assertEqual(2, len(parents))
            for parent in parents:
                self.assertRegex(git(clone, "rev-parse", "%s^{tree}" % parent), r"^[0-9a-f]{40}$")


if __name__ == "__main__":
    unittest.main()
