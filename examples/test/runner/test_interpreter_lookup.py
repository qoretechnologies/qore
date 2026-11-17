#!/usr/bin/env python3
"""Tests find the qore interpreter and qcc through examples/test/TestInterpreter.qm.

A test that runs the interpreter in a child process takes it from TestInterpreter::qore_interpreter() (and qcc from
TestInterpreter::qcc_compiler()), never from QORE_ARGV[0] or the environment (QORE_BINARY, QORE_BIN,
QORE_EXECUTABLE) itself: a test compiled with qcc is an executable, whose QORE_ARGV[0] is the test itself, which then
runs itself in its child processes without end.  TestInterpreter.qm never returns the executable of a compiled test,
and it stops a compiled test that its own child process runs again.

These tests fail when the code of a test source (not its strings or comments) reads QORE_ARGV[0] or one of those
environment variables, except in TestInterpreter.qm and in the sources listed in ALLOWED with the reason.

A test also takes the interpreter options of its own process from TestInterpreter::interpreter_options() (and its
execution mode from TestInterpreter::exec_mode()), never from the rest of QORE_ARGV: the arguments after the script are
the test's own (the -v of run_tests.sh -v), and a child interpreter given them as options refuses them (issue #6250).
These tests fail when the code of a test source reads QORE_ARGV other than QORE_ARGV[0], except in TestInterpreter.qm
and in the sources listed in ARGV_ALLOWED with the reason.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[3]
TESTS = ROOT / "examples/test"

#: the files of the code that a test compiled with qcc is made of: the test, the files that it includes and the modules
#: that it requires (scripts that the tests or the developers run with the interpreter, such as benchmarks and
#: generators, \c .q and \c .qr, are not compiled into a test)
SOURCE_SUFFIXES = (".qtest", ".qm", ".qc", ".qi", ".ql")

#: the lookups of the interpreter that only TestInterpreter.qm makes
LOOKUP = re.compile(r"\bQORE_ARGV\s*\[\s*0\s*\]"
                    r"|\bENV\s*(?:\.\s*\"?|\{\s*\")(?:QORE_BIN|QORE_BINARY|QORE_EXECUTABLE)\b"
                    r"|\bgetenv\(\s*\"(?:QORE_BIN|QORE_BINARY|QORE_EXECUTABLE)\"\s*\)")

#: the module that makes the lookups
INTERPRETER_MODULE = TESTS / "TestInterpreter.qm"

#: the sources that read a variable themselves, with the reason
ALLOWED = {
    # it checks the environment that run_tests.sh exports to the tests, which includes QORE_BINARY
    "examples/test/bin/run_tests/run-tests-credentials.qtest": "checks the variables that run_tests.sh exports",
}


#: the reads of QORE_ARGV other than QORE_ARGV[0], which only TestInterpreter.qm makes
ARGV_READ = re.compile(r"\bQORE_ARGV\b(?!\s*\[\s*0\s*\])")

#: the sources that read QORE_ARGV themselves, with the reason
ARGV_ALLOWED = {
    # qcc initializes the module when it compiles a test that requires it, without a command line
    "examples/test/QoreBuildTree.qm": "checks whether there is a command line at all",
    # its worker is the test itself, run with its whole command line (the script's own arguments after the script)
    "examples/test/qlib/DataProvider/PipelineMemory.qtest": "runs itself as its worker with its own command line",
}


def sources():
    """The Qore sources of the tests that run_tests.sh runs: those of examples/test and of the binary modules"""
    files = [p for p in TESTS.rglob("*") if p.suffix in SOURCE_SUFFIXES and p.is_file()]
    files += [p for p in (ROOT / "modules").glob("*/test/**/*") if p.suffix in SOURCE_SUFFIXES and p.is_file()]
    return sorted(files)


def code_spans(text):
    """Yields the (start, end) spans of code in Qore source: not string literals and not comments"""
    i, n, start = 0, len(text), 0
    while i < n:
        c = text[i]
        if c == "#" or (c == "/" and text.startswith("/*", i)):
            if start < i:
                yield (start, i)
            if c == "#":
                end = text.find("\n", i)
                i = n if end < 0 else end
            else:
                end = text.find("*/", i + 2)
                i = n if end < 0 else end + 2
            start = i
            continue
        if c in "\"'":
            if start < i:
                yield (start, i)
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            i = start = min(j + 1, n)
            continue
        i += 1
    if start < n:
        yield (start, n)


def lookups(text, pattern=LOOKUP):
    """Returns the line numbers and the text of the lookups in the code of a source (that start in code: the name that
    getenv() is given is a string)"""
    starts = list(code_spans(text))
    found = []
    for match in pattern.finditer(text):
        if any(start <= match.start() < end for start, end in starts):
            found.append((text.count("\n", 0, match.start()) + 1, match.group(0)))
    return found


class InterpreterLookupTest(unittest.TestCase):
    def test_tests_use_test_interpreter(self):
        errors = []
        for path in sources():
            rel = str(path.relative_to(ROOT))
            if path == INTERPRETER_MODULE or rel in ALLOWED:
                continue
            for line, text in lookups(path.read_text(errors="replace")):
                errors.append("%s:%d: %s: take the interpreter from TestInterpreter::qore_interpreter() and qcc from "
                              "TestInterpreter::qcc_compiler() (examples/test/TestInterpreter.qm)" % (rel, line, text))
        self.assertEqual([], errors, "\n".join(errors))

    def test_tests_take_interpreter_options_from_test_interpreter(self):
        errors = []
        for path in sources():
            rel = str(path.relative_to(ROOT))
            if path == INTERPRETER_MODULE or rel in ARGV_ALLOWED:
                continue
            for line, text in lookups(path.read_text(errors="replace"), ARGV_READ):
                errors.append("%s:%d: %s: take the interpreter options from TestInterpreter::interpreter_options() and "
                              "the execution mode from TestInterpreter::exec_mode() "
                              "(examples/test/TestInterpreter.qm): the arguments after the script are the test's own "
                              "(issue #6250)" % (rel, line, text))
        self.assertEqual([], errors, "\n".join(errors))

    def test_argv_allowed_sources_exist_and_need_it(self):
        for rel in ARGV_ALLOWED:
            path = ROOT / rel
            self.assertTrue(path.is_file(), "%s is listed in ARGV_ALLOWED but does not exist" % rel)
            self.assertTrue(lookups(path.read_text(errors="replace"), ARGV_READ), "%s is listed in ARGV_ALLOWED but "
                            "does not read QORE_ARGV" % rel)

    def test_argv_check_finds_reads_in_code_only(self):
        sample = "\n".join([
            "list<string> opts = map $1, QORE_ARGV[1..], $1 =~ /^-/;",
            "for (int i = 0; i < QORE_ARGV.size(); ++i) {}",
            "string a = QORE_ARGV[0];",
            "# QORE_ARGV[1] in a comment",
            "string b = \"QORE_ARGV in a string\";",
            "foreach string arg in (QORE_ARGV) {}",
        ])
        self.assertEqual([(1, "QORE_ARGV"), (2, "QORE_ARGV"), (6, "QORE_ARGV")], lookups(sample, ARGV_READ))

    def test_allowed_sources_exist_and_need_it(self):
        for rel in ALLOWED:
            path = ROOT / rel
            self.assertTrue(path.is_file(), "%s is listed in ALLOWED but does not exist" % rel)
            self.assertTrue(lookups(path.read_text(errors="replace")), "%s is listed in ALLOWED but makes no "
                            "lookup" % rel)

    def test_module_makes_the_lookups(self):
        self.assertTrue(lookups(INTERPRETER_MODULE.read_text()), "TestInterpreter.qm makes the lookups")

    def test_check_finds_lookups_in_code_only(self):
        sample = "\n".join([
            "string a = ENV.QORE_BIN ?? ENV.QORE_BINARY ?? QORE_ARGV[0] ?? \"qore\";",
            "string b = dirname(QORE_ARGV[0]) + \"/qcc\";",
            "string c = getenv(\"QORE_EXECUTABLE\");",
            "# QORE_ARGV[0] in a comment",
            "/* ENV.QORE_BINARY in a block comment */",
            "string d = \"QORE_ARGV[0] in a string \\\" ENV.QORE_BINARY\";",
            "string e = 'ENV.QORE_EXECUTABLE in a single-quoted string';",
            "string g = QORE_ARGV [ 0 ] + ENV{\"QORE_BIN\"} + ENV.\"QORE_BINARY\";",
            "string f = TestInterpreter::qore_interpreter();",
        ])
        self.assertEqual([(1, "ENV.QORE_BIN"), (1, "ENV.QORE_BINARY"), (1, "QORE_ARGV[0]"), (2, "QORE_ARGV[0]"),
                          (3, "getenv(\"QORE_EXECUTABLE\")"), (8, "QORE_ARGV [ 0 ]"), (8, "ENV{\"QORE_BIN"),
                          (8, "ENV.\"QORE_BINARY")], lookups(sample))


if __name__ == "__main__":
    unittest.main()
