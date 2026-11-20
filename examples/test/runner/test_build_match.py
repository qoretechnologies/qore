#!/usr/bin/env python3
"""run_tests.sh refuses to run the tests of a tree with a build made from other sources (#6897)

Each case makes a git repository with a few files of each kind (libqore's sources in lib/, a module in qlib/, a test
in examples/test/) and a stand-in build in build/: a qore script that names the commit in build/hash for --version and
runs a test file as a shell script, a libqore, a qcc, and the CMakeCache.txt of a build configured from the tree.  The
times of the files are set explicitly (a file changed after the build is newer than it), so no case waits for the
clock.  The runner tested is QORE_RUN_TESTS_SH if set (to show that a case fails with an older run_tests.sh), else the
one in this tree.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import textwrap
import unittest


ROOT = Path(__file__).resolve().parents[3]
RUN_TESTS = Path(os.environ.get("QORE_RUN_TESTS_SH", str(ROOT / "run_tests.sh"))).resolve()

# the time of the build's files, and of the sources made before it
BUILD_TIME = 2000000000
SOURCE_TIME = BUILD_TIME - 1000
# the time of a file changed after the build
LATER = BUILD_TIME + 1000

OTHER_COMMIT = "1234567890abcdef" * 2 + "12345678"


def git(cwd, *args):
    return subprocess.run(["git", "-c", "user.name=Qore runner test", "-c", "user.email=runner-test@invalid",
                           "-c", "init.defaultBranch=main", *args], cwd=cwd, check=True, capture_output=True,
                          text=True).stdout.strip()


def write_script(path, source):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(textwrap.dedent(source))
    path.chmod(0o755)
    return path


def stand_in_build(build, source_dir, commit):
    """Makes a stand-in build in the given directory, configured from source_dir, that names the given commit"""
    build.mkdir(parents=True, exist_ok=True)
    (build / "hash").write_text(commit + "\n")
    # qore: names the commit for --version, and runs the test file it is given as a shell script
    write_script(build / "qore", """\
        #!/bin/sh
        if [ "$1" = --version ]; then
            echo "QORE stand-in"
            echo "  git hash: `cat "$(dirname "$0")/hash"`"
            exit 0
        fi
        for arg in "$@"; do
            case "$arg" in
                *.qtest) exec sh "$arg" ;;
            esac
        done
        exit 2
        """)
    write_script(build / "qcc", """\
        #!/bin/sh
        echo "qcc stand-in"
        """)
    (build / "libqore.so").touch()
    (build / "CMakeCache.txt").write_text("CMAKE_HOME_DIRECTORY:INTERNAL=%s\n" % source_dir)
    (build / "qlib-qmod").mkdir(exist_ok=True)
    set_times(build, BUILD_TIME)


def set_times(path, when):
    """Sets the times of the file, or of the directory and everything in it, except .git"""
    path = Path(path)
    if path.is_dir():
        for directory, dirs, files in os.walk(path):
            dirs[:] = [d for d in dirs if d != ".git"]
            for name in files:
                full = os.path.join(directory, name)
                if not os.path.islink(full):
                    os.utime(full, (when, when))
            os.utime(directory, (when, when))
    else:
        os.utime(path, (when, when))


class BuildMatchTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="qore-runner-build-match-")
        self.base = Path(self.directory.name)
        self.tree = self.base / "tree"
        self.tree.mkdir()
        # run_tests.sh reads its helpers from the tree it runs in
        helpers = self.tree / "examples/test/docker_test"
        helpers.mkdir(parents=True)
        (helpers / "ci-job-processes.sh").write_text(
            (ROOT / "examples/test/docker_test/ci-job-processes.sh").read_text())
        (self.tree / "lib").mkdir()
        (self.tree / "lib/a.cpp").write_text("int a;\n")
        (self.tree / "lib/b.cpp").write_text("int b;\n")
        (self.tree / "qlib").mkdir()
        (self.tree / "qlib/Mod.qm").write_text("# module\n")
        (self.tree / "qlib/Src.qm").write_text("# module loaded from its source\n")
        (self.tree / "examples/test/tests").mkdir(parents=True)
        self.test = self.tree / "examples/test/tests/t.qtest"
        self.test.write_text("echo 'the test ran'\n")
        (self.tree / ".gitignore").write_text("/build/\n/cores/\n/tmp/\n/bin/\n")
        git(self.tree, "init", "-q")
        git(self.tree, "add", "-A")
        git(self.tree, "commit", "-q", "-m", "first")
        self.commit = git(self.tree, "rev-parse", "HEAD")
        set_times(self.tree, SOURCE_TIME)
        self.build = self.tree / "build"
        stand_in_build(self.build, self.tree, self.commit)
        # the build's qmod of Mod
        (self.build / "qlib-qmod/Mod.qmod").touch()
        os.utime(self.build / "qlib-qmod/Mod.qmod", (BUILD_TIME, BUILD_TIME))
        self.bin = self.tree / "bin"
        # sudo: fails; with CI set, run_tests.sh sets the core file name of a macOS machine with "sudo -n" where sudo
        # needs no password, which must not happen here
        write_script(self.bin / "sudo", """\
            #!/bin/sh
            exit 1
            """)
        (self.tree / "cores").mkdir()
        (self.tree / "tmp").mkdir()

    def tearDown(self):
        self.directory.cleanup()

    def run_runner(self, tree=None, build=None, **variables):
        tree = tree or self.tree
        build = build or self.build
        env = {
            "CI": "1",
            "HOME": os.environ.get("HOME", str(tree)),
            "PATH": str(self.bin) + os.pathsep + os.environ.get("PATH", os.defpath),
            "TMPDIR": str(tree / "tmp"),
            "QORE_BINARY": str(build / "qore"),
            "LIBQORE_BINARY": str(build / "libqore.so"),
            "CORE_DIR": str(tree / "cores"),
            "QORE_TEST_DIRS": "tests",
        }
        env.update(variables)
        result = subprocess.run(["sh", str(RUN_TESTS)], cwd=tree, env=env, capture_output=True, text=True,
                                timeout=120)
        result.output = result.stdout + result.stderr
        return result

    def assert_runs(self, result):
        self.assertEqual(0, result.returncode, result.output)
        self.assertIn("the test ran", result.output)
        self.assertNotIn("MISMATCH", result.output)

    def assert_refused(self, result, *lines):
        self.assertNotEqual(0, result.returncode, result.output)
        self.assertNotIn("the test ran", result.output)
        self.assertIn("*** MISMATCH: the build does not match the tree", result.output)
        self.assertIn("QORE_TEST_ALLOW_BUILD_MISMATCH=1 runs the tests all the same. Exiting.", result.output)
        for line in lines:
            self.assertIn(line, result.output)

    def commit_change(self, path, content, when):
        (self.tree / path).write_text(content)
        git(self.tree, "commit", "-q", "-am", "change " + path)
        os.utime(self.tree / path, (when, when))
        return git(self.tree, "rev-parse", "HEAD")

    def test_a_matching_build_runs(self):
        self.assert_runs(self.run_runner())

    def test_a_build_input_changed_after_the_build_is_refused(self):
        # an edit that was not built
        (self.tree / "lib/a.cpp").write_text("int a = 1;\n")
        os.utime(self.tree / "lib/a.cpp", (LATER, LATER))
        self.assert_refused(self.run_runner(), "1 files of the build changed since the build was made (from commit "
                            + self.commit, "    lib/a.cpp")

    def test_a_commit_checked_out_after_the_build_is_refused(self):
        head = self.commit_change("lib/a.cpp", "int a = 2;\n", LATER)
        self.assert_refused(self.run_runner(), self.commit, "the tree is at " + head, "    lib/a.cpp")

    def test_a_file_removed_after_the_build_is_refused(self):
        (self.tree / "lib/b.cpp").unlink()
        os.utime(self.tree / "lib", (LATER, LATER))
        self.assert_refused(self.run_runner(), "    lib/b.cpp")

    def test_a_change_that_was_built_runs(self):
        # an uncommitted change and a commit made before the build: the build is newer than both
        (self.tree / "lib/a.cpp").write_text("int a = 3;\n")
        os.utime(self.tree / "lib/a.cpp", (SOURCE_TIME, SOURCE_TIME))
        self.commit_change("lib/b.cpp", "int b = 3;\n", SOURCE_TIME)
        self.assert_runs(self.run_runner())

    def test_a_build_of_another_tree_is_refused_for_any_difference(self):
        # the same change, older than the build, in a build that was configured from another tree
        (self.tree / "lib/a.cpp").write_text("int a = 4;\n")
        os.utime(self.tree / "lib/a.cpp", (SOURCE_TIME, SOURCE_TIME))
        other = self.base / "other-build"
        stand_in_build(other, self.base / "other-tree", self.commit)
        self.assert_refused(self.run_runner(build=other), "and the build was not made from this tree",
                            "    lib/a.cpp")

    def test_a_module_is_compared_with_its_qmod(self):
        # Mod has a qmod in the build: a change after it is refused, unless the qmod has been made again since
        (self.tree / "qlib/Mod.qm").write_text("# module, changed\n")
        os.utime(self.tree / "qlib/Mod.qm", (LATER, LATER))
        self.assert_refused(self.run_runner(), "    qlib/Mod.qm")
        os.utime(self.build / "qlib-qmod/Mod.qmod", (LATER + 1, LATER + 1))
        self.assert_runs(self.run_runner())

    def test_a_module_without_a_qmod_is_its_source(self):
        (self.tree / "qlib/Src.qm").write_text("# module loaded from its source, changed\n")
        os.utime(self.tree / "qlib/Src.qm", (LATER, LATER))
        self.assert_runs(self.run_runner())

    def test_a_test_only_change_runs(self):
        # a test changed and committed after the build: HEAD is not the build's commit
        self.commit_change("examples/test/tests/t.qtest", "echo 'the test ran'; echo changed\n", LATER)
        (self.tree / "examples/test/tests/new.qtest").write_text("echo new\n")
        result = self.run_runner()
        self.assert_runs(result)
        self.assertIn("changed", result.output)

    def test_an_unknown_build_commit_is_refused(self):
        (self.build / "hash").write_text(OTHER_COMMIT + "\n")
        self.assert_refused(self.run_runner(), "the build's commit %s is not in the tree's repository (the tree is at "
                            "%s)" % (OTHER_COMMIT, self.commit))

    def test_a_build_that_names_no_commit_is_refused(self):
        (self.build / "hash").write_text("unknown\n")
        self.assert_refused(self.run_runner(), "does not name the commit it was made from")

    def test_a_qcc_of_another_build_is_refused(self):
        other = self.base / "other-build"
        stand_in_build(other, self.tree, OTHER_COMMIT)
        self.assert_refused(self.run_runner(QORE_QCC_EXECUTABLE=str(other / "qcc")),
                            "the qcc that the tests use (%s/qcc) comes from the build in %s, made from commit %s, not %s"
                            % (other, other.resolve(), OTHER_COMMIT, self.commit))
        # a qcc without a qore next to it cannot be told to be the build's
        lone = self.base / "lone"
        write_script(lone / "qcc", "#!/bin/sh\n")
        self.assert_refused(self.run_runner(QORE_QCC_EXECUTABLE=str(lone / "qcc")),
                            "which has no qore that names its commit")
        # one of the same commit is the build's
        stand_in_build(other, self.tree, self.commit)
        self.assert_runs(self.run_runner(QORE_QCC_EXECUTABLE=str(other / "qcc")))

    def test_qmod_files_of_another_build_are_refused(self):
        other = self.base / "other-build"
        stand_in_build(other, self.tree, OTHER_COMMIT)
        self.assert_refused(self.run_runner(QORE_TEST_QMOD_DIR=str(other / "qlib-qmod")),
                            "the .qmod files in %s/qlib-qmod comes from the build in" % other.resolve())
        # a qlib/*.qmod link into another build
        (self.tree / "qlib/Mod.qmod").symlink_to(other / "qlib-qmod/Mod.qmod")
        self.assert_refused(self.run_runner(), "the qlib/*.qmod links into %s" % other)
        # links into the build itself
        (self.tree / "qlib/Mod.qmod").unlink()
        (self.tree / "qlib/Mod.qmod").symlink_to(self.build / "qlib-qmod/Mod.qmod")
        self.assert_runs(self.run_runner())

    def archive(self, marker):
        """Makes a copy of the tree without its repository, with the given .git-archive-revision (None: none)"""
        archive = self.base / "archive"
        shutil.copytree(self.tree, archive, symlinks=True, ignore=shutil.ignore_patterns(".git", "build"))
        if marker is not None:
            (archive / ".git-archive-revision").write_text(marker)
        return archive

    def test_an_archive_needs_the_build_s_commit(self):
        archive = self.archive(self.commit + "\n")
        self.assert_runs(self.run_runner(tree=archive))
        shutil.rmtree(archive)
        archive = self.archive(OTHER_COMMIT + "\n")
        self.assert_refused(self.run_runner(tree=archive), "the tree was made from commit %s "
                            "(.git-archive-revision), the build from %s" % (OTHER_COMMIT, self.commit))
        for marker in (None, "$Format:%H$\n"):
            with self.subTest(marker=marker):
                shutil.rmtree(archive)
                archive = self.archive(marker)
                self.assert_refused(self.run_runner(tree=archive), "the tree has no git repository and no "
                                    ".git-archive-revision naming the commit it was made from")

    def test_the_override_runs_the_tests_and_says_so(self):
        (self.build / "hash").write_text(OTHER_COMMIT + "\n")
        result = self.run_runner(QORE_TEST_ALLOW_BUILD_MISMATCH="1")
        self.assertEqual(0, result.returncode, result.output)
        self.assertIn("*** MISMATCH: the build does not match the tree", result.output)
        self.assertIn("*** the tests run all the same: QORE_TEST_ALLOW_BUILD_MISMATCH=1 ***", result.output)
        self.assertIn("the test ran", result.output)


if __name__ == "__main__":
    unittest.main()
