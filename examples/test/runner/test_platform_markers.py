#!/usr/bin/env python3
"""The tests whose checks depend on the platform declare each platform beside Linux that they must also run on.

The Linux CI jobs run every test.  The macOS CI job runs only the tests that declare, on a line of their own,

    # test-platform: macos - <why>

(run_tests.sh with QORE_TEST_PLATFORM_SUBSET=macos, which test-macos.sh sets): the tests of the OS-facing code (files,
sockets, TLS, processes and signals, threads, sandboxing, module and dylib loading, time zones, encodings, stack
limits, the JIT and AOT code generation and linking), whose checks can pass on Linux and fail on macOS.  A test of
platform-independent logic declares nothing, as the Linux jobs make its checks.

These tests fail when a declaration is malformed (another form, an unknown platform, no reason, or more than one
for a platform), when a file that is not a test declares one (run_tests.sh reads only the test files), when no test
declares macOS, when run_tests.sh does not select exactly the tests that declare it, or when a CI job other than the
macOS job selects a subset.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
TESTS = ROOT / "examples/test"

#: the platforms that a test may declare
PLATFORMS = ("macos",)

#: a declaration: the platform and the reason, which says what the test checks that depends on the platform
MARKER = re.compile(r"^# test-platform: (?P<platform>[a-z0-9]+) - (?P<why>\S(?:.*\S)?)$")
#: the files other than tests that Qore code is in: modules, classes, include files and scripts
SOURCE_SUFFIXES = (".q", ".qr", ".qm", ".qc", ".qi", ".ql", ".qclass", ".qconst", ".qfd", ".qsd", ".qsm", ".qwf")
#: any line that looks like a declaration, which must then be one
ANY_MARKER = re.compile(r"^\s*#+\s*test[-_ ]?platforms?\s*:", re.IGNORECASE)


def all_tests():
    """The tests that run_tests.sh runs: those of examples/test and of the binary modules"""
    tests = sorted(TESTS.rglob("*.qtest"))
    tests += sorted((ROOT / "modules").glob("*/test/**/*.qtest"))
    return tests


def check_declarations(path):
    """Returns the platforms that a file declares and a list of errors in its declarations"""
    platforms = []
    errors = []
    for number, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
        if not ANY_MARKER.match(line):
            continue
        where = "%s:%d" % (path, number)
        match = MARKER.match(line)
        if not match:
            errors.append("%s: a declaration has the form '# test-platform: <platform> - <why>' on a line of its own: "
                          "%r" % (where, line))
            continue
        platform = match.group("platform")
        if platform not in PLATFORMS:
            errors.append("%s: unknown platform %r (known: %s)" % (where, platform, ", ".join(PLATFORMS)))
            continue
        if platform in platforms:
            errors.append("%s: the platform %r is declared more than once" % (where, platform))
            continue
        platforms.append(platform)
    return platforms, errors


def declared(platform):
    """The tests that declare the given platform, as run_tests.sh names them"""
    return [test for test in all_tests() if platform in check_declarations(test)[0]]


class PlatformDeclarationTest(unittest.TestCase):
    def test_declarations_are_well_formed(self):
        errors = []
        for test in all_tests():
            errors += check_declarations(test)[1]
        self.assertEqual([], errors)

    def test_only_tests_declare(self):
        # run_tests.sh reads the declaration of the test file only: one in a file that a test includes or requires
        # selects nothing
        found = []
        for directory in (TESTS, ROOT / "modules"):
            for path in sorted(directory.rglob("*")):
                if path.suffix not in SOURCE_SUFFIXES or not path.is_file():
                    continue
                if any(ANY_MARKER.match(line) for line in path.read_text(errors="replace").splitlines()):
                    found.append(str(path.relative_to(ROOT)))
        self.assertEqual([], found, "only a test file (*.qtest) declares a platform")

    def test_macos_selection(self):
        tests = declared("macos")
        # the selection is not empty, and a small part of the tests: the Linux jobs run all of them
        self.assertGreater(len(tests), 100)
        self.assertLess(len(tests), len(all_tests()) / 2)
        # it covers each kind of OS-facing code: the directories of the tests of files, sockets, processes and
        # threads, sandboxing, module loading, the JIT and AOT, time zones and the binary modules
        names = [str(test.relative_to(ROOT)) for test in tests]
        for part in ("examples/test/qore/classes/File/", "examples/test/qore/classes/Socket/",
                     "examples/test/qore/threads/", "examples/test/qore/security/", "examples/test/qore/misc/",
                     "examples/test/qlib/HttpServer/", "examples/test/ir/", "examples/test/module-cpp-api/",
                     "modules/"):
            with self.subTest(part=part):
                self.assertTrue(any(name.startswith(part) for name in names), "no test of %s declares macOS" % part)

    def test_check_finds_malformed_declarations(self):
        with tempfile.TemporaryDirectory(prefix="qore-platform-markers-") as directory:
            test = Path(directory) / "case.qtest"
            cases = {
                "# test-platform: macos - non-blocking pipe reads and EAGAIN in the File class": (["macos"], 0),
                "# test-platform: macos -": ([], 1),
                "# test-platform: macos - ": ([], 1),
                "# test-platform: macos": ([], 1),
                "# test-platform: macos - trailing space ": ([], 1),
                "#test-platform: macos - no space after the hash": ([], 1),
                "  # test-platform: macos - indented": ([], 1),
                "# test-platform: macOS - the platform is lower-case": ([], 1),
                "# test-platform: windows - an unknown platform": ([], 1),
                "# test-platforms: macos - the plural": ([], 1),
                "# test-platform: macos, linux - a list": ([], 1),
                "# test-platform: macos - sockets\n# test-platform: macos - again": (["macos"], 1),
                # other comments that mention the declaration are not declarations
                "# a test that declares test-platform: macos runs on macOS": ([], 0),
                "%modern": ([], 0),
            }
            for text, (platforms, errors) in cases.items():
                with self.subTest(text=text):
                    test.write_text("#!/usr/bin/env qore\n" + text + "\n")
                    found, found_errors = check_declarations(test)
                    self.assertEqual(platforms, found)
                    self.assertEqual(errors, len(found_errors), found_errors)

    def test_ci_selects_only_on_macos(self):
        # the macOS job sets the subset, after its build; no other CI job or script does, so the Linux jobs run every
        # test
        macos = (ROOT / "examples/test/docker_test/test-macos.sh").read_text()
        self.assertRegex(macos, r'(?m)^QORE_TEST_PLATFORM_SUBSET="\$\{QORE_TEST_PLATFORM_SUBSET-macos\}"$')
        self.assertRegex(macos, r"(?m)^export QORE_TEST_PLATFORM_SUBSET$")
        self.assertLess(macos.index("export QORE_TEST_PLATFORM_SUBSET"), macos.index("./run_tests.sh"))
        self.assertGreater(macos.index("export QORE_TEST_PLATFORM_SUBSET"), macos.index("make -j"))
        self.assertNotIn("QORE_TEST_PLATFORM_SUBSET", (ROOT / ".gitlab-ci.yml").read_text())
        for script in sorted((ROOT / "examples/test/docker_test").glob("*.sh")):
            if script.name != "test-macos.sh":
                with self.subTest(script=script.name):
                    self.assertNotIn("QORE_TEST_PLATFORM_SUBSET", script.read_text())

    def test_runner_lists_the_declared_tests(self):
        # run_tests.sh -l in the macOS job's selection lists exactly the tests that declare macOS (the binary modules'
        # tests are named in QORE_TEST_EXTRA_DIRS, as test-macos.sh names them), and every test without it
        extra = " ".join(sorted(str(d.relative_to(ROOT)) for d in (ROOT / "modules").glob("*/test") if d.is_dir()))
        env = {"PATH": os.defpath, "QORE_TEST_EXTRA_DIRS": extra}

        def listed(subset):
            run_env = dict(env)
            if subset is not None:
                run_env["QORE_TEST_PLATFORM_SUBSET"] = subset
            result = subprocess.run(["sh", "./run_tests.sh", "-l"], cwd=ROOT, env=run_env, capture_output=True,
                                    text=True, timeout=300)
            self.assertEqual(0, result.returncode, result.stderr)
            return sorted(re.sub(r"^\./", "", line) for line in result.stdout.split())

        everything = sorted(str(test.relative_to(ROOT)) for test in all_tests())
        self.assertEqual(everything, listed(None))
        self.assertEqual(everything, listed(""))
        self.assertEqual(sorted(str(test.relative_to(ROOT)) for test in declared("macos")), listed("macos"))


class RunnerSelectionTest(unittest.TestCase):
    """run_tests.sh with a stand-in for qore that records the tests it runs"""

    def run_runner(self, env_extra, files=None):
        with tempfile.TemporaryDirectory(prefix="qore-runner-platform-") as directory:
            root = Path(directory)
            tests = root / "examples/test"
            (tests / "docker_test").mkdir(parents=True)
            (tests / "docker_test/ci-job-processes.sh").symlink_to(
                ROOT / "examples/test/docker_test/ci-job-processes.sh")
            if files is None:
                files = {
                    "macos.qtest": "# test-platform: macos - sockets and TLS over loopback\n",
                    "sub/macos2.qtest": "# -*- mode: qore -*-\n# test-platform: macos - fork() and exit codes\n",
                    "plain.qtest": "# says test-platform: macos - in a comment\n",
                    "malformed.qtest": "#test-platform: macos - no space after the hash\n",
                    "noreason.qtest": "# test-platform: macos -\n",
                }
            for name, text in files.items():
                (tests / name).parent.mkdir(parents=True, exist_ok=True)
                (tests / name).write_text("#!/usr/bin/env qore\n" + text)
            binaries = root / "bin"
            binaries.mkdir()
            (binaries / "libqore.so").touch()
            log = root / "ran"
            qore = binaries / "qore"
            qore.write_text('#!/bin/sh\nfor a in "$@"; do case "$a" in *.qtest) basename "$a" >> "$RAN_LOG";; esac; '
                            'done\nexit 0\n')
            qore.chmod(0o755)
            env = {
                "PATH": os.defpath,
                "CI": "1",
                "QORE_BINARY": str(qore),
                "LIBQORE_BINARY": str(binaries / "libqore.so"),
                "CORE_DIR": str(root / "cores"),
                "RAN_LOG": str(log),
            }
            env.update(env_extra)
            result = subprocess.run(["sh", str(ROOT / "run_tests.sh")], cwd=root, env=env, capture_output=True,
                                    text=True, timeout=120)
            ran = sorted(log.read_text().split()) if log.exists() else []
            return result, ran

    def test_no_subset_runs_every_test(self):
        for env in ({}, {"QORE_TEST_PLATFORM_SUBSET": ""}):
            with self.subTest(env=env):
                result, ran = self.run_runner(env)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertEqual(["macos.qtest", "macos2.qtest", "malformed.qtest", "noreason.qtest", "plain.qtest"],
                                 ran)
                self.assertNotIn("test-platform", result.stdout)

    def test_macos_subset_runs_only_declared_tests(self):
        result, ran = self.run_runner({"QORE_TEST_PLATFORM_SUBSET": "macos"})
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        # a malformed declaration selects nothing (and test_declarations_are_well_formed fails on it)
        self.assertEqual(["macos.qtest", "macos2.qtest"], ran)
        self.assertIn("Selected 2 of 5 tests that declare 'test-platform: macos'", result.stdout)

    def test_subset_applies_to_named_tests_and_parallel_runs(self):
        result, ran = self.run_runner({"QORE_TEST_PLATFORM_SUBSET": "macos", "QORE_TEST_JOBS": "3"})
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertEqual(["macos.qtest", "macos2.qtest"], ran)
        self.assertIn("Running 2 tests, 2 at a time", result.stdout)

    def test_subset_without_declared_tests_fails(self):
        result, ran = self.run_runner({"QORE_TEST_PLATFORM_SUBSET": "macos"},
                                      files={"plain.qtest": "%modern\n"})
        self.assertNotEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn("ERROR: no tests found to run", result.stderr)
        self.assertEqual([], ran)

    def test_unknown_subset_fails(self):
        for subset in ("linux", "MACOS", "macos ", "1"):
            with self.subTest(subset=subset):
                result, ran = self.run_runner({"QORE_TEST_PLATFORM_SUBSET": subset})
                self.assertEqual(1, result.returncode, result.stdout + result.stderr)
                self.assertIn("ERROR: QORE_TEST_PLATFORM_SUBSET is empty or a platform", result.stderr)
                self.assertEqual([], ran)


if __name__ == "__main__":
    unittest.main()
