#!/usr/bin/env python3
"""The assert methods of QUnit return no value: a test never uses what one returns.

A QUnit assert method (assertTrue(), assertEq(), ...) records a failed assertion and returns nothing.  Code that uses
its result, such as

    if (!assertTrue(is_file(exe), "built the executable")) { continue; }

always takes the branch (the negation of no value is true), so the checks after it never run, and the test passes
without them: the qcc executable checks of two tests were skipped that way.  An assert method is called as a statement;
a test that needs the condition afterwards tests the condition itself:

    assertTrue(is_file(exe), "built the executable");
    if (!is_file(exe)) { continue; }

These tests fail when the code (not the strings or comments) of a test source or of a module in qlib uses the result
of a QUnit assert method: after '!', '&&', '||', '?', an assignment or comparison, 'return', or as an argument or a
condition.  A call as a statement, an element of a 'map' expression that discards its results, and methods of other
classes whose names begin with 'assert' are not concerned; the names of the assert methods are read from QUnit.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[3]

#: the QUnit module, which declares the assert methods
QUNIT = ROOT / "qlib/QUnit.qm"

#: the files of Qore code that are scanned
SOURCE_SUFFIXES = (".qtest", ".qm", ".qc", ".qi", ".ql", ".q", ".qr")


def assert_names(text):
    """The names of the assert methods that QUnit declares without a return type (that return no value)"""
    return sorted(set(re.findall(r"^\s+public\s+(assert[A-Za-z]+)\s*\(", text, re.M)))


def sources():
    """The test sources and the modules of qlib"""
    files = []
    for d in ("examples/test", "qlib"):
        files += [p for p in (ROOT / d).rglob("*") if p.suffix in SOURCE_SUFFIXES and p.is_file()]
    files += [p for p in (ROOT / "modules").glob("*/test/**/*") if p.suffix in SOURCE_SUFFIXES and p.is_file()]
    return sorted(files)


def code_mask(text):
    """Returns a mask of the characters of Qore code: not string literals and not comments ('$#' is code)"""
    mask = bytearray(len(text))
    i, n, start = 0, len(text), 0

    def code(a, b):
        for k in range(a, b):
            mask[k] = 1

    while i < n:
        c = text[i]
        if (c == "#" and not (i and text[i - 1] == "$")) or (c == "/" and text.startswith("/*", i)):
            code(start, i)
            if c == "#":
                end = text.find("\n", i)
                i = n if end < 0 else end
            else:
                end = text.find("*/", i + 2)
                i = n if end < 0 else end + 2
            start = i
            continue
        if c in "\"'":
            code(start, i)
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            i = start = min(j + 1, n)
            continue
        i += 1
    code(start, n)
    return mask


def used_results(text, names):
    """Returns the line numbers and the source lines of the calls of the assert methods whose result is used"""
    if not names:
        return []
    call = re.compile(r"(?<![\w.:])(?:(?:self|[A-Za-z_]\w*)\s*(?:\.|::)\s*)?\b(" + "|".join(names) + r")\s*\(")
    mask = code_mask(text)
    lines = text.splitlines()
    found = []
    for match in call.finditer(text):
        start = match.start()
        if not mask[start]:
            continue
        # the code before the call, skipping whitespace, strings and comments
        k = start - 1
        while k >= 0 and (text[k].isspace() or not mask[k]):
            k -= 1
        if k < 0:
            continue
        prev = text[k]
        w = k
        while w >= 0 and (text[w].isalnum() or text[w] == "_"):
            w -= 1
        word = text[w + 1:k + 1]
        two = text[k - 1:k + 1] if k >= 1 else ""
        used = False
        if word == "return":
            used = True
        elif not word:
            if prev in "!?=([," or two in ("&&", "||"):
                used = True
            elif prev == ":" and two != "::":
                # the alternative of a ternary operator; a label or a case is a statement
                used = "?" in text[text.rfind("\n", 0, k) + 1:k]
        if used:
            line = text.count("\n", 0, start) + 1
            found.append((line, lines[line - 1].strip()))
    return found


class QUnitAssertResultTest(unittest.TestCase):
    def test_qunit_declares_assert_methods(self):
        names = assert_names(QUNIT.read_text())
        self.assertIn("assertTrue", names)
        self.assertIn("assertEq", names)

    def test_no_source_uses_an_assert_result(self):
        names = assert_names(QUNIT.read_text())
        errors = []
        for path in sources():
            text = path.read_text(errors="replace")
            if "assert" not in text:
                continue
            for line, source in used_results(text, names):
                errors.append("%s:%d: the result of a QUnit assert method is used, but it returns no value: %s"
                              % (path.relative_to(ROOT), line, source))
        self.assertEqual([], errors, "\n".join(errors))

    def test_check_finds_used_results_only(self):
        names = ["assertEq", "assertTrue"]
        sample = "\n".join([
            "if (!assertTrue(is_file(exe), \"built\")) { continue; }",           # 1: used
            "bool b = assertEq(1, x);",                                          # 2: used
            "return assertTrue(x);",                                             # 3: used
            "foo(assertEq(1, 2));",                                              # 4: used
            "if (x && assertTrue(y)) {}",                                        # 5: used
            "auto v = c ? assertTrue(x) : 0;",                                   # 6: used
            "assertTrue(x, \"a statement\");",                                   # 7: not used
            "self.assertEq(1, x);",                                              # 8: not used
            "map assertEq($1, $1), list;",                                       # 9: not used
            "if (x) assertTrue(y);",                                             # 10: not used
            "# if (!assertTrue(x)) in a comment",                                # 11: not used
            "string s = \"if (!assertTrue(x))\";",                               # 12: not used
            "string r = client.assertServerType(t);",                            # 13: not used (another method)
            "public assertEq(auto expected, auto actual) {",                     # 14: not used (a declaration)
            "printf(\"%d\\n\", $#); assertEq(1, x);",                            # 15: not used ($# is code)
        ])
        self.assertEqual([1, 2, 3, 4, 5, 6], [line for line, _ in used_results(sample, names)])


if __name__ == "__main__":
    unittest.main()
