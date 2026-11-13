#!/usr/bin/env python3
"""The tests that use examples/test/ExecModeHarness.qm declare whether the execution mode of the test itself matters.

A harness test runs child processes that each set their own execution mode (--exec-mode).  One whose checks are all
made in those child processes declares

    # test-exec-mode: children - <why>

and the CI jobs that give every test an execution mode (QORE_TEST_OPTS with --exec-mode) leave it out with
QORE_TEST_EXCLUDE_EXEC_MODE_CHILDREN=1 (run_tests.sh): the default-mode jobs run the same child processes, so it would
repeat their checks.  One that also makes checks in its own process (code run in its own Program, results computed
in its own process, its own options or mode given to a child process) declares

    # test-exec-mode: parent - <why>

and runs in every job.  A test that does not use the harness may also declare "children" when it makes its checks in
child processes of its own binary (QORE_ARGV[0]) that it starts without an execution mode: they run in the default
mode in every job, so the default-mode jobs make the same checks.  These tests fail when a harness test declares
neither, when a test that does not use the harness declares "parent" or declares "children" without starting such
child processes, when a test that declares "children" does what only a "parent" test may do, when the CI jobs set
QORE_TEST_EXCLUDE_EXEC_MODE_CHILDREN in a job whose tests get no execution mode, and they check that run_tests.sh
leaves out exactly the "children" tests.

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

MARKER = re.compile(r"(?m)^# test-exec-mode: (?P<kind>\S+) - (?P<why>\S.*)$")
ANY_MARKER = re.compile(r"(?m)^\s*#\s*test-exec-mode:")
HARNESS = re.compile(r"(?m)^\s*%requires\s+\S*ExecModeHarness(\.qm)?\s*$")
INCLUDE = re.compile(r"(?m)^\s*%include\s+(\S+)\s*$")

# what the code of a "children" test may not do in its own process (strings and comments do not count): read or set
# its own execution mode, read its own options (QORE_ARGV beyond the binary, QORE_TEST_OPTS), run code in a Program
# of its own, run the checks modules and include files whose results the tests compare in their own process, run
# the C++ unit tests or fork
FORBIDDEN = re.compile(r"\b(getExecMode|setExecMode|QORE_TEST_OPTS|callFunction|callMethod|callStaticMethod|"
                       r"run_debug_unit_tests|get_results|get_lines|fork)\b|\bQORE_ARGV(?!\[0\])")
# a Program of its own may only parse code: a parse error does not depend on the execution mode
PROGRAM = re.compile(r"\bProgram\b")
PROGRAM_RUN = re.compile(r"\.\s*run\s*\(")


def strip_code(source):
    """Returns Qore source without its comments and with its strings emptied"""
    out = []
    i = 0
    n = len(source)
    while i < n:
        c = source[i]
        if c == "#":
            j = source.find("\n", i)
            i = n if j < 0 else j
            continue
        if source.startswith("/*", i):
            j = source.find("*/", i + 2)
            i = n if j < 0 else j + 2
            out.append(" ")
            continue
        if c == '"':
            i += 1
            while i < n and source[i] != '"':
                i += 2 if source[i] == "\\" else 1
            i += 1
            out.append('""')
            continue
        if c == "'":
            j = source.find("'", i + 1)
            i = n if j < 0 else j + 1
            out.append("''")
            continue
        out.append(c)
        i += 1
    return "".join(out)


def sources(test):
    """Returns the test and the files it includes (recursively), which run in the test's process"""
    seen = []
    todo = [test]
    while todo:
        path = todo.pop()
        if path in seen or not path.is_file():
            continue
        seen.append(path)
        for match in INCLUDE.finditer(path.read_text(errors="replace")):
            todo.append((path.parent / match.group(1)).resolve())
    return seen


# a child process of the test's own binary: QORE_ARGV[0] given to a command it runs (strings and comments do not count)
CHILD_BINARY = re.compile(r"\bQORE_ARGV\[0\]")


def starts_children(test):
    """Returns True if the code of a test or of the files it includes runs its own binary in a child process"""
    for path in sources(test):
        code = strip_code(path.read_text(errors="replace"))
        if CHILD_BINARY.search(code) and re.search(r"\b(backquote|system|exec)\s*\(|\bProcess\b", code):
            return True
    return False


def uses_harness(test):
    return any(HARNESS.search(path.read_text(errors="replace")) for path in sources(test))


def all_tests():
    tests = sorted(TESTS.rglob("*.qtest"))
    tests += sorted((ROOT / "modules").glob("*/test/**/*.qtest"))
    return tests


def parent_only_code(test):
    """Returns what the code of a test or of the files it includes does that only a "parent" test may do"""
    found = []
    for path in sources(test):
        code = strip_code(path.read_text(errors="replace"))
        found += sorted({m.group(0) for m in FORBIDDEN.finditer(code)})
        if PROGRAM.search(code) and PROGRAM_RUN.search(code):
            found.append("Program ... .run()")
    return found


class ExecModeMarkerTest(unittest.TestCase):
    def test_harness_tests_declare_marker(self):
        harness_tests = 0
        kinds = {"children": 0, "parent": 0}
        non_harness_children = []
        for test in all_tests():
            text = test.read_text(errors="replace")
            name = test.relative_to(ROOT)
            markers = MARKER.findall(text)
            self.assertEqual(len(ANY_MARKER.findall(text)), len(markers),
                             "%s: a test-exec-mode declaration has the form '# test-exec-mode: <kind> - <why>'" % name)
            if not uses_harness(test):
                if markers:
                    self.assertEqual([("children", markers[0][1])], markers, "%s does not use ExecModeHarness.qm: "
                                     "it declares no test-exec-mode, or 'children' once" % name)
                    self.assertTrue(starts_children(test), "%s does not use ExecModeHarness.qm: it declares "
                                    "'children' only if it runs its own binary (QORE_ARGV[0]) in child processes"
                                    % name)
                    non_harness_children.append(str(name))
                continue
            harness_tests += 1
            self.assertEqual(1, len(markers), "%s uses ExecModeHarness.qm: it declares '# test-exec-mode: children "
                             "- <why>' or '# test-exec-mode: parent - <why>' once" % name)
            kind = markers[0][0]
            self.assertIn(kind, kinds, "%s: the kind is 'children' or 'parent'" % name)
            kinds[kind] += 1
        # the harness tests are found, and most of them are child-process tests
        self.assertGreater(harness_tests, 200)
        self.assertGreater(kinds["children"], kinds["parent"])
        self.assertGreater(kinds["parent"], 0)
        # the tests that run their checks in default-mode child processes without the harness:
        # - AOTConstantImportStale (#5798) and AOTDumpInfoPendingConstants (#5799): their checks run qcc and the
        #   programs and modules it compiles in child processes
        # - GenericCacheRelease: GenericCacheRelease.qr runs in child processes that set their own --exec-mode
        # - RestSchemaModuleLazyTypes-<n>: the modules are loaded and compared in child processes
        # - WebSocketProgramEnd: the server whose Program ends with an open connection is a child process; the
        #   WebSocket client in the test's own process only drives it
        self.assertEqual(sorted(["examples/test/ir/AOTConstantImportStale.qtest",
                                 "examples/test/ir/AOTDumpInfoPendingConstants.qtest",
                                 "examples/test/ir/GenericCacheRelease.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-1.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-2.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-3.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-4.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-5.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-6.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-7.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-8.qtest",
                                 "examples/test/qlib/WebSocketHandler/WebSocketProgramEnd.qtest"]),
                         sorted(non_harness_children))

    def test_children_tests_check_nothing_in_their_own_process(self):
        for test in all_tests():
            match = MARKER.search(test.read_text(errors="replace"))
            if not match or match.group("kind") != "children":
                continue
            with self.subTest(test=str(test.relative_to(ROOT))):
                self.assertEqual([], parent_only_code(test),
                                 "a test that declares 'test-exec-mode: children' makes all its checks in child "
                                 "processes; this code runs in its own process, so it declares 'parent'")

    def test_scan_finds_parent_checks(self):
        # the scan finds each kind of check in the test's own process, and not the same text in a string or comment
        with tempfile.TemporaryDirectory(prefix="qore-exec-mode-markers-") as directory:
            root = Path(directory)
            (root / "checks.qi").write_text("sub f() { ChecksModule::get_results(); }\n")
            cases = {
                "string sub mode() { return getExecMode(); }": ["getExecMode"],
                "string opts = ENV.QORE_TEST_OPTS;": ["QORE_TEST_OPTS"],
                "string cmd = QORE_ARGV.join(\" \");": ["QORE_ARGV"],
                "string arg = QORE_ARGV[1];": ["QORE_ARGV"],
                "Program p(); p.parse(src, \"x\"); p.callFunction(\"f\");": ["callFunction"],
                "Program p(); p.parse(src, \"x\"); p.run();": ["Program ... .run()"],
                "%include checks.qi\n": ["get_results"],
                # only the binary, a parse in a Program of its own, and text in strings and comments are allowed
                "string qore = QORE_ARGV[0];": [],
                "Program p(); p.parse(src, \"x\");": [],
                "string src = \"Program p(); p.callFunction('f'); getExecMode();\";": [],
                "# getExecMode() and QORE_ARGV[1] in a comment\n/* p.callFunction() */": [],
                "string s = 'getExecMode()';": [],
            }
            for code, expected in cases.items():
                with self.subTest(code=code):
                    test = root / "case.qtest"
                    test.write_text("%modern\n" + code + "\n")
                    self.assertEqual(expected, parent_only_code(test))

    def test_scan_finds_child_processes(self):
        # a test that runs its own binary in a child process is found, and not the same text in a string or comment
        with tempfile.TemporaryDirectory(prefix="qore-exec-mode-children-") as directory:
            root = Path(directory)
            cases = {
                "string out = backquote(QORE_ARGV[0] + \" child.q\");": True,
                "int rc = system(sprintf(\"%s child.q\", QORE_ARGV[0]));": True,
                "string qore = QORE_ARGV[0];": False,
                "string out = backquote(\"ls\");": False,
                "# backquote(QORE_ARGV[0])\nstring s = \"system(QORE_ARGV[0])\";": False,
            }
            for code, expected in cases.items():
                with self.subTest(code=code):
                    test = root / "case.qtest"
                    test.write_text("%modern\n" + code + "\n")
                    self.assertEqual(expected, starts_children(test))

    def test_ci_excludes_only_in_explicit_mode_jobs(self):
        text = (ROOT / ".gitlab-ci.yml").read_text()
        jobs = re.split(r"(?m)^(?=[a-z][\w-]*:\s*$)", text)
        excluding = []
        for job in jobs:
            name = job.split(":", 1)[0]
            opts = re.search(r"(?m)^\s+QORE_TEST_OPTS:\s*'([^']*)'", job)
            exclude = re.search(r'(?m)^\s+QORE_TEST_EXCLUDE_EXEC_MODE_CHILDREN:\s*"?1"?\s*$', job)
            explicit = bool(opts and re.search(r"--exec-mode[= ]", opts.group(1)))
            if explicit or exclude:
                with self.subTest(job=name):
                    self.assertEqual(explicit, bool(exclude), "a job leaves out the child-process tests exactly "
                                     "when its tests get an execution mode")
            if exclude:
                excluding.append(name)
        self.assertEqual(["test-ast-ubuntu-amd64", "test-ir-ubuntu-amd64", "test-jit-ubuntu-amd64",
                          "test-tiered-ubuntu-amd64"], excluding)
        # the global variables give no execution mode, so the default-mode jobs run every test
        global_variables = re.search(r"(?ms)^variables:\n.*?^\S", text)
        self.assertIsNotNone(global_variables)
        self.assertNotRegex(global_variables.group(0), r"QORE_TEST_EXCLUDE_EXEC_MODE_CHILDREN|--exec-mode")


class RunnerExclusionTest(unittest.TestCase):
    """run_tests.sh with a stand-in for qore that records the tests it runs"""

    def run_runner(self, env_extra):
        with tempfile.TemporaryDirectory(prefix="qore-runner-exec-mode-") as directory:
            root = Path(directory)
            tests = root / "examples/test"
            (tests / "docker_test").mkdir(parents=True)
            (tests / "docker_test/ci-job-processes.sh").symlink_to(
                ROOT / "examples/test/docker_test/ci-job-processes.sh")
            (tests / "children.qtest").write_text("#!/usr/bin/env qore\n# test-exec-mode: children - child processes\n")
            (tests / "parent.qtest").write_text("#!/usr/bin/env qore\n# test-exec-mode: parent - own process\n")
            (tests / "plain.qtest").write_text("#!/usr/bin/env qore\n# says test-exec-mode: children in a comment\n")
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

    def test_default_mode_runs_every_test(self):
        result, ran = self.run_runner({"QORE_TEST_OPTS": "-penable-debug"})
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertEqual(["children.qtest", "parent.qtest", "plain.qtest"], ran)

    def test_explicit_mode_without_exclusion_runs_every_test(self):
        result, ran = self.run_runner({"QORE_TEST_OPTS": "-penable-debug --exec-mode=jit"})
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertEqual(["children.qtest", "parent.qtest", "plain.qtest"], ran)

    def test_exclusion_leaves_out_children_tests(self):
        for opts in ("-penable-debug --exec-mode=ir --ir-fallback-warn", "--exec-mode tiered"):
            with self.subTest(opts=opts):
                result, ran = self.run_runner({"QORE_TEST_OPTS": opts, "QORE_TEST_EXCLUDE_EXEC_MODE_CHILDREN": "1"})
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertEqual(["parent.qtest", "plain.qtest"], ran)
                self.assertIn("Left out 1 tests that declare 'test-exec-mode: children'", result.stdout)

    def test_exclusion_needs_an_execution_mode(self):
        for opts in ("-penable-debug", "", "--exec-modes=jit"):
            with self.subTest(opts=opts):
                result, ran = self.run_runner({"QORE_TEST_OPTS": opts, "QORE_TEST_EXCLUDE_EXEC_MODE_CHILDREN": "1"})
                self.assertEqual(1, result.returncode, result.stdout + result.stderr)
                self.assertIn("needs an --exec-mode option in QORE_TEST_OPTS", result.stderr)
                self.assertEqual([], ran)


if __name__ == "__main__":
    unittest.main()
