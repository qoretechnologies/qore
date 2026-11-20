#!/usr/bin/env python3
"""A closure given to addTestCase() inside a foreach loop must not use the loop variable (qore#6636).

A Qore closure binds a local variable, not its value, and the variable of a foreach loop is one variable for all
iterations: when QUnit runs the test cases after the loop, every such closure reads the variable's last value, so a
test that adds one case per execution mode runs the last mode under the name of each.  The loop variable must be
copied into a local declared in the loop's body, or given to the test case as its argument
(addTestCase(name, \\method(), value)).

This test fails for every test script whose foreach loop gives addTestCase() a closure (sub ...) that uses the loop
variable.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]

FOREACH = re.compile(r"\bforeach\s+(?:[\w:<>*,\s]+?\s+)?(\w+)\s+in\s*\(")
ADD_CLOSURE = re.compile(r"\baddTestCase\s*\(")


def block_after(text, pos):
    """returns the text of the braced block that starts at the first '{' at or after pos, or ''"""
    start = text.find("{", pos)
    if start < 0:
        return ""
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if not depth:
                return text[start:i + 1]
    return text[start:]


def strip_comments(text):
    return re.sub(r"#[^\n]*", "", text)


def offenders(text):
    """returns the loop variables of the foreach loops whose addTestCase() closure uses them"""
    text = strip_comments(text)
    rv = []
    for m in FOREACH.finditer(text):
        var = m.group(1)
        body = block_after(text, m.end())
        for a in ADD_CLOSURE.finditer(body):
            # the arguments of the call: up to its closing parenthesis
            depth = 0
            end = len(body)
            for i in range(a.end() - 1, len(body)):
                if body[i] == "(":
                    depth += 1
                elif body[i] == ")":
                    depth -= 1
                    if not depth:
                        end = i
                        break
            args = body[a.end():end]
            closure = re.search(r"\bsub\s*\(", args)
            if not closure:
                continue
            # a local of the loop's body that copies the variable is declared before the call: the closure uses the
            # copy, so only a direct use of the loop variable in the closure counts
            if re.search(r"\b%s\b" % re.escape(var), args[closure.start():]):
                rv.append(var)
                break
    return rv


class AddTestCaseLoopClosuresTest(unittest.TestCase):
    maxDiff = None

    def test_no_loop_variable_in_closures(self):
        bad = []
        for path in sorted((ROOT / "examples/test").rglob("*.qtest")):
            found = offenders(path.read_text(errors="replace"))
            if found:
                bad.append("%s (loop variable %s)" % (path.relative_to(ROOT), ", ".join(found)))
        self.assertEqual([], bad, "a closure given to addTestCase() in a foreach loop uses the loop variable, so "
                         "every case sees its last value (qore#6636): copy it into a local of the loop's body or "
                         "give it as the test case's argument")

    def test_detects_the_pattern(self):
        self.assertEqual(["mode"], offenders("""
foreach string mode in (("ast", "ir")) {
    addTestCase(mode, sub () { checkMode(mode); });
}"""))
        # a copy in the loop's body, or the value as the argument, is fine
        self.assertEqual([], offenders("""
foreach string mode in (("ast", "ir")) {
    string m = mode;
    addTestCase(mode, sub () { checkMode(m); });
}"""))
        self.assertEqual([], offenders("""
foreach string mode in (("ast", "ir")) {
    addTestCase(mode, \\checkMode(), mode);
}"""))


if __name__ == "__main__":
    unittest.main()
