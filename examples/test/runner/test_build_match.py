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
import sys
import textwrap
import unittest


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_watchdog import end_session, session_processes  # noqa: E402
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
    # qore: names the commit for --version: that of the libqore it loads, the first in LD_LIBRARY_PATH that has one, or
    # else the one next to it, as a qore linked with libqore finds it; with a version latch, it waits for it first (a
    # libqore that hangs in its initialization).  It runs the test file it is given as a shell script.
    write_script(build / "qore", """\
        #!/bin/sh
        if [ "$1" = --version ]; then
            if [ -n "$VERSION_LATCH" ]; then
                read -r line < "$VERSION_LATCH"
            fi
            lib=$(dirname "$0")
            for dir in `echo "$LD_LIBRARY_PATH" | tr : ' '`; do
                if [ -f "$dir/libqore.so" ] && [ -f "$dir/hash" ]; then
                    lib=$dir
                    break
                fi
            done
            echo "QORE stand-in"
            echo "  git hash: `cat "$lib/hash"`"
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
        # in a session of its own, whose processes are ended once it has ended or when it fails: a runner that does not
        # limit the time of the check leaves a qore that hangs running (test_a_qore_that_hangs_is_ended())
        process = subprocess.Popen(["sh", str(RUN_TESTS)], cwd=tree, env=env, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True, start_new_session=True)
        try:
            stdout, stderr = process.communicate(timeout=120)
        finally:
            end_session(process)
        self.assertEqual([], session_processes(process.pid), "a process of the run is left")
        result = subprocess.CompletedProcess(process.args, process.returncode, stdout, stderr)
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
        self.assert_refused(self.run_runner(), "1 file(s) of the build changed since the build was made (from commit "
                            + self.commit, "    lib/a.cpp")

    def test_a_commit_checked_out_after_the_build_is_refused(self):
        head = self.commit_change("lib/a.cpp", "int a = 2;\n", LATER)
        self.assert_refused(self.run_runner(), self.commit, "the tree is at " + head, "    lib/a.cpp")

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
                            "the qcc that the tests use (%s/qcc): from the build in %s, made from commit %s, not %s"
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
                            "the .qmod files in %s/qlib-qmod: from the build in" % other.resolve())
        # a qlib/*.qmod link into another build
        (self.tree / "qlib/Mod.qmod").symlink_to(other / "qlib-qmod/Mod.qmod")
        self.assert_refused(self.run_runner(), "the qlib/*.qmod links into %s" % other)
        # links into the build itself
        (self.tree / "qlib/Mod.qmod").unlink()
        (self.tree / "qlib/Mod.qmod").symlink_to(self.build / "qlib-qmod/Mod.qmod")
        self.assert_runs(self.run_runner())

    def test_another_libqore_in_the_library_path_does_not_name_the_build_s_commit(self):
        # the caller's LD_LIBRARY_PATH names another libqore (the image's own, older Qore in a CI job): the build's
        # qore names the commit of the libqore that the tests load, the build's, which the runner puts first
        other = self.base / "other-lib"
        other.mkdir()
        (other / "libqore.so").touch()
        (other / "hash").write_text(OTHER_COMMIT + "\n")
        self.assert_runs(self.run_runner(LD_LIBRARY_PATH=str(other)))
        # a qcc of another build directory of the same commit: the qore next to it names the commit of its own libqore
        companion = self.base / "companion-build"
        stand_in_build(companion, self.tree, self.commit)
        self.assert_runs(self.run_runner(LD_LIBRARY_PATH=str(other), QORE_QCC_EXECUTABLE=str(companion / "qcc")))

    def test_a_qore_that_hangs_is_ended(self):
        # qore --version waits for a latch that nothing writes (a libqore that hangs in its initialization): the
        # check ends it after its time limit and refuses the run
        latch = self.base / "version-latch"
        os.mkfifo(latch)
        self.assert_refused(self.run_runner(VERSION_LATCH=str(latch), QORE_TEST_BUILD_CHECK_WAIT="1"),
                            "does not name the commit it was made from")

    def test_a_reconfigure_after_an_edit_does_not_hide_it(self):
        # CMakeCache.txt and .ninja_log are written by a configure and by any ninja command, without a build of
        # libqore: an edit of libqore's source that came after libqore is refused all the same
        (self.tree / "lib/a.cpp").write_text("int a = 5;\n")
        os.utime(self.tree / "lib/a.cpp", (LATER, LATER))
        for name in ("CMakeCache.txt", ".ninja_log"):
            (self.build / name).touch()
            os.utime(self.build / name, (LATER + 10, LATER + 10))
        self.assert_refused(self.run_runner(), "    lib/a.cpp")

    def test_a_cmake_file_is_compared_with_the_configuration(self):
        (self.tree / "cmake").mkdir()
        (self.tree / "cmake/Foo.cmake").write_text("# changed\n")
        os.utime(self.tree / "cmake/Foo.cmake", (LATER, LATER))
        git(self.tree, "add", "cmake/Foo.cmake")
        self.assert_refused(self.run_runner(), "    cmake/Foo.cmake")
        os.utime(self.build / "CMakeCache.txt", (LATER + 10, LATER + 10))
        self.assert_runs(self.run_runner())

    def test_a_source_is_compared_with_what_the_tests_use(self):
        # a binary module's source is compared with the module's file, a test program's source with its object file
        for source, obj in (("modules/m/src/m.cpp", "modules/m/m-api-1.0.qmod"),
                            ("examples/test/qore/x/y.cpp", "CMakeFiles/y-test.dir/examples/test/qore/x/y.cpp.o")):
            with self.subTest(source=source):
                (self.tree / source).parent.mkdir(parents=True, exist_ok=True)
                (self.tree / source).write_text("// changed\n")
                git(self.tree, "add", source)
                os.utime(self.tree / source, (LATER, LATER))
                (self.build / obj).parent.mkdir(parents=True, exist_ok=True)
                (self.build / obj).touch()
                os.utime(self.build / obj, (BUILD_TIME, BUILD_TIME))
                self.assert_refused(self.run_runner(), "    " + source)
                os.utime(self.build / obj, (LATER + 10, LATER + 10))
                self.assert_runs(self.run_runner())

    def test_files_that_the_build_makes_nothing_from_are_not_compared(self):
        # a binary module's tests, and a source under examples/ that CMake does not compile (no object file)
        for path in ("modules/m/test/m.qtest", "examples/test/cmake/samples/sample.cpp"):
            (self.tree / path).parent.mkdir(parents=True, exist_ok=True)
            (self.tree / path).write_text("// changed\n")
            git(self.tree, "add", path)
            os.utime(self.tree / path, (LATER, LATER))
        self.assert_runs(self.run_runner())

    def test_git_s_messages_are_not_file_names(self):
        # a git that warns on a diff that works: its message is not taken for a changed file (which led to the tree's
        # root, which a CI job changes when it makes crash-dumps/); git runs without fetching (GIT_NO_LAZY_FETCH)
        real_git = shutil.which("git")
        lazy = self.base / "lazy-fetch"
        write_script(self.bin / "git", f"""\
            #!/bin/sh
            echo "$GIT_NO_LAZY_FETCH" >> '{lazy}'
            echo "warning: a message of git" >&2
            exec '{real_git}' "$@"
            """)
        os.utime(self.tree, (LATER, LATER))
        self.assert_runs(self.run_runner())
        self.assertEqual({"1"}, set(lazy.read_text().split()))

    def test_qmod_links_are_read_with_a_readlink_of_one_file(self):
        # BusyBox's readlink takes one file: the links into another build are found all the same
        other = self.base / "other-build"
        stand_in_build(other, self.tree, OTHER_COMMIT)
        for name in ("Mod", "Src"):
            (self.tree / ("qlib/%s.qmod" % name)).symlink_to(other / ("qlib-qmod/%s.qmod" % name))
        real_readlink = shutil.which("readlink")
        write_script(self.bin / "readlink", f"""\
            #!/bin/sh
            [ $# -eq 1 ] || {{ echo "usage: readlink FILE" >&2; exit 1; }}
            exec '{real_readlink}' "$@"
            """)
        self.assert_refused(self.run_runner(), "the qlib/*.qmod links into %s" % other)

    def test_an_installed_qcc_is_not_the_build_s(self):
        # a qcc whose qore has no libqore next to it (an installed one) names the commit of the libqore that its own
        # loader finds, not that of the build's libqore in the tests' library path (which comes first on Linux)
        installed = self.base / "installed" / "bin"
        stand_in_build(installed, self.tree, OTHER_COMMIT)
        (installed / "libqore.so").unlink()
        self.assert_refused(self.run_runner(QCC=str(installed / "qcc")), "the qcc that the tests use (%s/qcc): from the "
                            "build in %s, made from commit %s" % (installed, installed.resolve(), OTHER_COMMIT))

    def test_a_header_of_an_example_module_is_compared_with_its_objects(self):
        # an example module built by add_subdirectory() has its objects in the build directory of its own directory
        header = self.tree / "examples/test/mod/mod.h"
        header.parent.mkdir(parents=True)
        header.write_text("// changed\n")
        git(self.tree, "add", str(header))
        os.utime(header, (LATER, LATER))
        obj = self.build / "examples/test/mod/CMakeFiles/mod.dir/mod.cpp.o"
        obj.parent.mkdir(parents=True)
        obj.touch()
        os.utime(obj, (BUILD_TIME, BUILD_TIME))
        self.assert_refused(self.run_runner(), "    examples/test/mod/mod.h")
        os.utime(obj, (LATER + 10, LATER + 10))
        self.assert_runs(self.run_runner())

    def test_objects_of_helper_targets_are_not_compared(self):
        # a helper target that compiles libqore's sources once more (built once, and not by a normal build) keeps an
        # old object: libqore's file is compared with libqore only
        for target, when in (("qore-ir-phase0-smoke.dir", BUILD_TIME), ("libqore.dir", LATER + 10)):
            obj = self.build / "CMakeFiles" / target / "lib/a.cpp.o"
            obj.parent.mkdir(parents=True)
            obj.touch()
            os.utime(obj, (when, when))
        (self.tree / "lib/a.cpp").write_text("int a = 6;\n")
        os.utime(self.tree / "lib/a.cpp", (LATER, LATER))
        os.utime(self.build / "libqore.so", (LATER + 10, LATER + 10))
        self.assert_runs(self.run_runner())
        os.utime(self.build / "libqore.so", (BUILD_TIME, BUILD_TIME))
        self.assert_refused(self.run_runner(), "    lib/a.cpp")

    def test_removed_files_are_not_compared(self):
        # files removed from libqore, qlib, a binary module and a test program, with what a build left of them (an
        # object, a .qmod, a module directory) older than their directories, and an editor's file in lib/: none of
        # their directories' times is a build's
        (self.tree / "qlib/Gone.qm").write_text("# gone\n")
        (self.tree / "modules/foo/src").mkdir(parents=True)
        (self.tree / "modules/foo/src/foo.cpp").write_text("// foo\n")
        (self.tree / "examples/test/foo").mkdir(parents=True)
        (self.tree / "examples/test/foo/foo.cpp").write_text("int main() {}\n")
        git(self.tree, "add", "-A")
        git(self.tree, "commit", "-q", "-m", "more")
        (self.build / "hash").write_text(git(self.tree, "rev-parse", "HEAD") + "\n")
        for leftover in ("qlib-qmod/Gone.qmod", "modules/foo/foo-api-1.0.qmod",
                         "CMakeFiles/libqore.dir/lib/b.cpp.o", "CMakeFiles/foo-test.dir/examples/test/foo/foo.cpp.o"):
            (self.build / leftover).parent.mkdir(parents=True, exist_ok=True)
            (self.build / leftover).touch()
            os.utime(self.build / leftover, (BUILD_TIME, BUILD_TIME))
        for path in ("qlib/Gone.qm", "lib/b.cpp", "modules/foo", "examples/test/foo/foo.cpp"):
            git(self.tree, "rm", "-rq", path)
        (self.tree / "lib/.a.cpp.swp").write_text("an editor's file\n")
        for directory in ("qlib", "lib", "modules", "examples/test/foo"):
            if (self.tree / directory).exists():
                os.utime(self.tree / directory, (LATER, LATER))
        self.assert_runs(self.run_runner())

    def test_a_file_removed_from_a_module_in_qlib_is_compared_with_its_qmod(self):
        # the module's other sources are still there: their directory is compared with the module's .qmod
        sep = self.tree / "qlib/Sep"
        sep.mkdir()
        (sep / "X.qc").write_text("# x\n")
        (sep / "Y.qc").write_text("# y\n")
        git(self.tree, "add", "-A")
        git(self.tree, "commit", "-q", "-m", "sep")
        (self.build / "hash").write_text(git(self.tree, "rev-parse", "HEAD") + "\n")
        qmod = self.build / "qlib-qmod/Sep.qmod"
        qmod.touch()
        os.utime(qmod, (BUILD_TIME, BUILD_TIME))
        git(self.tree, "rm", "-q", "qlib/Sep/X.qc")
        os.utime(sep, (LATER, LATER))
        self.assert_refused(self.run_runner(), "    qlib/Sep/X.qc")
        os.utime(qmod, (LATER + 10, LATER + 10))
        self.assert_runs(self.run_runner())

    def test_headers_of_example_modules_in_other_directories_are_compared(self):
        # a header in include/ with the sources at the module's top, and a header at the top with the sources in src/
        for header, obj in (("examples/test/mod/include/mod.h", "examples/test/mod/CMakeFiles/mod.dir/mod.cpp.o"),
                            ("examples/test/mod2/mod2.h", "examples/test/mod2/CMakeFiles/mod2.dir/src/mod2.cpp.o")):
            with self.subTest(header=header):
                path = self.tree / header
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("// changed\n")
                git(self.tree, "add", header)
                os.utime(path, (LATER, LATER))
                (self.build / obj).parent.mkdir(parents=True, exist_ok=True)
                (self.build / obj).touch()
                os.utime(self.build / obj, (BUILD_TIME, BUILD_TIME))
                self.assert_refused(self.run_runner(), "    " + header)
                os.utime(self.build / obj, (LATER + 10, LATER + 10))
                self.assert_runs(self.run_runner())

    def test_the_check_leaves_the_tests_environment_as_it_is(self):
        # GIT_NO_LAZY_FETCH is for the check's git only: "VAR=value function" leaves it set and exported in bash
        self.test.write_text("echo 'the test ran'\nif env | grep '^GIT_NO_LAZY_FETCH='; then echo 'leaked'; fi\n")
        result = self.run_runner()
        self.assert_runs(result)
        self.assertNotIn("leaked", result.output)

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
