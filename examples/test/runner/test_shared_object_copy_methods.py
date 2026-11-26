#!/usr/bin/env python3
"""A class that guards its own state with SandboxManager::isSharedObject(self) declares its own copy() method.

copy() of an object that trusted code marked as shared (SandboxManager::shareObject(), qore#6843) is refused for code
that is sandboxed or acts for sandboxed code unless a class of the copy declares a copy() method in Qore code, and the
copy() method that allows it must reset what the class guards for a shared object, as the copy is an ordinary object
with copies of all members, also the private ones (qore#7667).  A copy() method of a base class cannot know the state
of a subclass, so every class whose code guards state for a shared object declares its own copy() method.

These tests scan the Qore sources of the standard modules (qlib) and of the binary modules (modules/*) for the
classes whose code guards state for a shared object (it calls SandboxManager::isSharedObject(self), reads the
shared_object member of the classes of the data provider types and REST schemas, or calls their helpers
isOrphanedSharedObject() and isOutsideSharedTypeBuild()) and fail for each one whose declaration has no copy() method,
or only an empty one: a copy() method with no code declares that its class guards no state of its own with the
comment "no own guarded state" in its body.  Methods defined out of line (Class::method()) are not scanned.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]
SOURCE_DIRS = (ROOT / "qlib", ROOT / "modules")
SUFFIXES = (".qm", ".qc", ".ql")

CLASS = re.compile(r"^\s*(?:(?:public|private(?::\w+)?|final|abstract)\s+)*class\s+([A-Za-z_]\w*)\b")
COPY = re.compile(r"^\s*(?:(?:public|private(?::\w+)?|synchronized)\s+)*copy\s*\(")
GUARD = re.compile(r"\bisSharedObject\s*\(\s*self\s*\)|\bshared_object\b|\bisOrphanedSharedObject\s*\(|"
                   r"\bisOutsideSharedTypeBuild\s*\(")
#! the comment of a copy() method with no code, whose class guards no state of its own
NO_OWN_STATE = "no own guarded state"
#! a regular expression literal: its text can hold a '#', a quote or a brace
REGEX = re.compile(r"[=!]~\s*(s|x|tr)?/")


def strip(text):
    """Returns the text without comments and string contents, keeping its lines."""
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        m = REGEX.match(text, i) if c in "=!" else None
        if m:
            j = m.end()
            for _ in range(2 if m.group(1) in ("s", "tr") else 1):
                while j < n and text[j] != "/" and text[j] != "\n":
                    j += 2 if text[j] == "\\" else 1
                j += 1
            out.append("=~//")
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("\n" * text.count("\n", i, j))
            i = j
        elif c == "#":
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif c == '"' or c == "'":
            j = i + 1
            while j < n and text[j] != c:
                if text[j] == "\\":
                    j += 1
                elif text[j] == "\n" and c == "'":
                    break
                j += 1
            out.append(c + "\n" * text.count("\n", i + 1, min(j, n)) + c)
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def scan_text(text):
    """Returns {class name: declares copy()} for the classes of the text whose own code guards state for a shared
    object; a copy() method counts if it has code or the comment NO_OWN_STATE."""
    found = {}
    depth = 0
    # the open classes: [name, depth of its body, declares copy(), guards]
    classes = []
    pending = None
    # the copy() method being read: [class, first line]
    copy_method = None
    raw = text.split("\n")
    lines = strip(text).split("\n")
    for i, line in enumerate(lines):
        m = CLASS.match(line)
        if m:
            pending = m.group(1)
        if pending and "{" in line:
            classes.append([pending, depth + 1, False, False])
            pending = None
            depth += line.count("{") - line.count("}")
            continue
        if classes:
            cls = classes[-1]
            if depth == cls[1] and COPY.match(line):
                copy_method = [cls, i]
            if GUARD.search(line):
                cls[3] = True
        depth += line.count("{") - line.count("}")
        if copy_method and depth == copy_method[0][1] and "}" in line:
            body = "\n".join(lines[copy_method[1]:i + 1])
            code = body[body.find("{") + 1:body.rfind("}")]
            if code.strip() or NO_OWN_STATE in "\n".join(raw[copy_method[1]:i + 1]):
                copy_method[0][2] = True
            copy_method = None
        while classes and depth < classes[-1][1]:
            name, _, declares_copy, guards = classes.pop()
            if guards:
                found[name] = declares_copy
    return found


def scan(unbalanced=None):
    """Returns {(path, class name): declares copy()} for the guarding classes of the module sources.

    The files scanned whose braces do not balance once comments, strings and regular expressions are removed, which
    the scan cannot read, are appended to unbalanced.
    """
    found = {}
    for top in SOURCE_DIRS:
        for path in sorted(top.rglob("*")):
            if path.suffix not in SUFFIXES or not path.is_file() or "/test/" in path.as_posix():
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            if not GUARD.search(text):
                continue
            rel = path.relative_to(ROOT).as_posix()
            stripped = strip(text)
            if unbalanced is not None and stripped.count("{") != stripped.count("}"):
                unbalanced.append(rel)
            for name, declares_copy in scan_text(text).items():
                found[(rel, name)] = declares_copy
    return found


class SharedObjectCopyMethodsTest(unittest.TestCase):
    def test_scan_text(self):
        src = """
public class A {
    /** @code{.py} { @endcode */
    bool f() { return SandboxManager::isSharedObject(self); }
    copy() {
        string s = "}";
    }
}
class B inherits A {
    private {
        int x;
    }
    g() {
        if (SandboxManager::isSharedObject(self)) { # }
            x = 1;
            bool b = ("#{" =~ /^#{"/ && "}" !~ s/}#/'/g);
        }
    }
    h() {
        copy();
    }
}
class C {
    bool f(object o) { return SandboxManager::isSharedObject(o); }
}
class D {
    private { bool shared_object; }
    copy() {
    }
}
class E inherits D {
    bool f() { return isOrphanedSharedObject(); }
    copy(E old) {
        # no own guarded state
    }
}
class F {
    bool f() { return isOutsideSharedTypeBuild(); }
    copy() {}
}
"""
        self.assertEqual({"A": True, "B": False, "D": False, "E": True, "F": False}, scan_text(src))

    def test_scan_finds_guarding_classes(self):
        found = scan()
        names = {k[1] for k in found}
        for name in ("AbstractDataField", "AbstractDataProviderType", "AbstractRestSchemaObject",
                     "AbstractRestSchemaValidator", "OpenApi3Schema", "SwaggerSchema", "SchemaObject"):
            self.assertIn(name, names, "the scan finds the classes that call isSharedObject(self)")

    def test_scan_reads_the_sources(self):
        unbalanced = []
        scan(unbalanced)
        self.assertEqual([], unbalanced, "sources whose braces the scan cannot match: extend strip()")

    def test_guarding_classes_declare_copy(self):
        missing = ["%s %s" % k for k, declares_copy in sorted(scan().items()) if not declares_copy]
        self.assertEqual([], missing, "classes that guard state for a shared object and declare no copy() method "
                         "that resets it (see this test's description)")


if __name__ == "__main__":
    unittest.main()
