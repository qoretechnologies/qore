#!/usr/bin/env python3
"""The tests whose checks apply only to a release build declare it, so that CI runs them on its release build.

The CI test jobs test a debug build; the only release build is the performance job's (build-perf, test-perf).  A test
whose checks apply only to a release build (the release stack budgets of qore/stack/recursion-frame-budget.qtest)
declares, on a line of its own,

    # test-release-build: <why>

and run_tests.sh -P (QORE_PERF_TESTS_ONLY=1, which test-perf sets) selects it with the performance tests; -E (the test
jobs) still runs it (issue #6540).  The declaration has no value, only the reason.

These tests fail when a declaration is malformed (another form, a misspelling, no reason, or more than one), when a
file that is not a test declares one (run_tests.sh reads only the test files), when no test declares it, or when
run_tests.sh -P does not select exactly the performance tests and the tests that declare it.

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

#: a declaration: the reason, which says why the test's checks apply only to a release build
MARKER = re.compile(r"^# test-release-build: (?P<why>\S(?:.*\S)?)$")
#: the files other than tests that Qore code is in: modules, classes, include files and scripts
SOURCE_SUFFIXES = (".q", ".qr", ".qm", ".qc", ".qi", ".ql", ".qclass", ".qconst", ".qfd", ".qsd", ".qsm", ".qwf")
#: any line that looks like a declaration, which must then be one
ANY_MARKER = re.compile(r"^\s*#+\s*test[-_ ]?release[-_ ]?builds?\b", re.IGNORECASE)


def all_tests():
    """The tests that run_tests.sh runs: those of examples/test and of the binary modules"""
    tests = sorted(TESTS.rglob("*.qtest"))
    tests += sorted((ROOT / "modules").glob("*/test/**/*.qtest"))
    return tests


def check_declarations(path):
    """Returns whether a file declares that it checks a release build, and a list of errors in its declarations"""
    found = False
    errors = []
    for number, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
        if not ANY_MARKER.match(line):
            continue
        where = "%s:%d" % (path, number)
        if not MARKER.match(line):
            errors.append("%s: a declaration has the form '# test-release-build: <why>' on a line of its own: %r"
                          % (where, line))
            continue
        if found:
            errors.append("%s: the declaration is made more than once" % where)
            continue
        found = True
    return found, errors


def declared():
    """The tests that declare that they check a release build"""
    return [test for test in all_tests() if check_declarations(test)[0]]


def is_perf_test(path):
    """The performance tests, as run_tests.sh selects them by name (is_perf_test())"""
    return "Perf" in path.name or path.name == "PipelineMemory.qtest"


class ReleaseBuildDeclarationTest(unittest.TestCase):
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
        self.assertEqual([], found, "only a test file (*.qtest) declares that it checks a release build")

    def test_recursion_frame_budget_declares(self):
        self.assertIn(TESTS / "qore/stack/recursion-frame-budget.qtest", declared())

    def test_check_finds_malformed_declarations(self):
        with tempfile.TemporaryDirectory(prefix="qore-release-build-markers-") as directory:
            test = Path(directory) / "case.qtest"
            cases = {
                "# test-release-build: its release budgets are checked only in a release build": (True, 0),
                "# test-release-build:": (False, 1),
                "# test-release-build: ": (False, 1),
                "# test-release-build: trailing space ": (False, 1),
                "# test-release-build - the form of the other markers": (False, 1),
                "# test-release-build: yes - a value": (True, 0),
                "#test-release-build: no space after the hash": (False, 1),
                "  # test-release-build: indented": (False, 1),
                "## test-release-build: two hashes": (False, 1),
                "# Test-Release-Build: capitals": (False, 1),
                "# test_release_build: underscores": (False, 1),
                "# test-release-builds: the plural": (False, 1),
                "# test release build: spaces": (False, 1),
                "# test-releasebuild: no dash": (False, 1),
                "# test-release-build:no space after the colon": (False, 1),
                "# test-release-build: budgets\n# test-release-build: again": (True, 1),
                # other comments that mention the declaration are not declarations
                "# a test that declares test-release-build: runs in the performance job": (False, 0),
                "%modern": (False, 0),
            }
            for text, (found, errors) in cases.items():
                with self.subTest(text=text):
                    test.write_text("#!/usr/bin/env qore\n" + text + "\n")
                    found_decl, found_errors = check_declarations(test)
                    self.assertEqual(found, found_decl)
                    self.assertEqual(errors, len(found_errors), found_errors)

    def test_runner_selects_the_declared_tests(self):
        # run_tests.sh -P lists exactly the performance tests and the tests that declare a release build; -E lists
        # every test but the performance tests, so the declared tests also run in the test jobs
        extra = " ".join(sorted(str(d.relative_to(ROOT)) for d in (ROOT / "modules").glob("*/test") if d.is_dir()))

        def listed(option):
            result = subprocess.run(["sh", "./run_tests.sh", option, "-l"], cwd=ROOT, capture_output=True, text=True,
                                    timeout=300, env={"PATH": os.defpath, "QORE_TEST_EXTRA_DIRS": extra})
            self.assertEqual(0, result.returncode, result.stderr)
            return sorted(re.sub(r"^\./", "", line) for line in result.stdout.split())

        tests = all_tests()
        name = lambda test: str(test.relative_to(ROOT))
        release = {name(test) for test in declared()}
        self.assertEqual(sorted({name(t) for t in tests if is_perf_test(t)} | release), listed("-P"))
        self.assertEqual(sorted(name(t) for t in tests if not is_perf_test(t)), listed("-E"))


class RunnerSelectionTest(unittest.TestCase):
    """run_tests.sh -P with a stand-in for qore that records the tests it runs: it selects what the check accepts"""

    def test_selection_follows_the_form(self):
        with tempfile.TemporaryDirectory(prefix="qore-runner-release-build-") as directory:
            root = Path(directory)
            tests = root / "examples/test"
            (tests / "docker_test").mkdir(parents=True)
            (tests / "docker_test/ci-job-processes.sh").symlink_to(
                ROOT / "examples/test/docker_test/ci-job-processes.sh")
            files = {
                "release.qtest": "# test-release-build: budgets of a release build\n",
                "sub/release2.qtest": "# -*- mode: qore -*-\n# test-release-build: release-only assertions\n",
                "FooPerf.qtest": "%modern\n",
                "plain.qtest": "# says test-release-build: in a comment\n",
                "nospace.qtest": "#test-release-build: no space after the hash\n",
                "noreason.qtest": "# test-release-build: \n",
                "trailing.qtest": "# test-release-build: trailing space \n",
                "otherform.qtest": "# test-release-build - the form of the other markers\n",
            }
            for file_name, text in files.items():
                (tests / file_name).parent.mkdir(parents=True, exist_ok=True)
                (tests / file_name).write_text("#!/usr/bin/env qore\n" + text)
            # the check accepts exactly the tests that the runner selects
            accepted = sorted(Path(n).name for n in files if check_declarations(tests / n)[0])
            self.assertEqual(["release.qtest", "release2.qtest"], accepted)
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
            for option, expected in (("-P", ["FooPerf.qtest"] + accepted),
                                     ("-E", sorted(Path(n).name for n in files if n != "FooPerf.qtest"))):
                with self.subTest(option=option):
                    if log.exists():
                        log.unlink()
                    result = subprocess.run(["sh", str(ROOT / "run_tests.sh"), option], cwd=root, env=env,
                                            capture_output=True, text=True, timeout=120)
                    self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                    self.assertEqual(sorted(expected), sorted(log.read_text().split()))


if __name__ == "__main__":
    unittest.main()
