#!/usr/bin/env python3
"""A public header does not declare a non-inline function that the library keeps hidden (qore#4380).

A function declared DLLLOCAL in a header of include/qore/ (outside include/qore/intern/) and defined in the library is not
exported: a module that includes the header sees a declaration that it cannot link to (through a derived class or an
inline function that calls it), and the header documents an interface that is not one.  Such an operation belongs to the
private implementation (include/qore/intern/), or is inline, or is exported on purpose.

This test finds the declarations (a DLLLOCAL function that is not inline, not pure virtual, not defaulted or deleted) and
fails when one is not listed in examples/test/runner/public_header_dlllocal_allowlist.txt.  The allowlist holds the
declarations that are still to be moved and the ones that stay on purpose (each with its reason); it only shrinks: the
test also fails for an entry that no header declares any more, so that a fix removes its entry.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[3]
INCLUDE = ROOT / "include/qore"
ALLOWLIST = Path(__file__).resolve().parent / "public_header_dlllocal_allowlist.txt"

CLASS_HEAD = re.compile(r"\b(?:class|struct)\s+(?:DLLEXPORT\s+|DLLLOCAL\s+)?([A-Za-z_]\w*)\s*(?:final\s*)?(?::[^{;]*)?\{")


def blank(text):
    """Comments and string literals become spaces (the offsets stay the same)"""
    def spaces(match):
        return re.sub(r"[^\n]", " ", match.group(0))

    text = re.sub(r"/\*.*?\*/", spaces, text, flags=re.S)
    text = re.sub(r"//[^\n]*", spaces, text)
    return re.sub(r'"(?:\\.|[^"\\\n])*"', spaces, text)


def public_headers():
    """The headers of include/qore/ outside include/qore/intern/"""
    for directory, subdirectories, files in os.walk(INCLUDE):
        relative = Path(directory).relative_to(INCLUDE)
        if "intern" in relative.parts:
            subdirectories[:] = []
            continue
        for name in sorted(files):
            if name.endswith(".h"):
                yield Path(directory) / name


def declarations(path):
    """The non-inline DLLLOCAL function declarations of a header as (class, declaration) pairs"""
    text = blank(path.read_text(errors="replace"))
    spans = []
    for match in CLASS_HEAD.finditer(text):
        start = match.end() - 1
        depth = 0
        end = len(text) - 1
        for position in range(start, len(text)):
            if text[position] == "{":
                depth += 1
            elif text[position] == "}":
                depth -= 1
                if depth == 0:
                    end = position
                    break
        spans.append((start, end, match.group(1)))
    found = []
    for match in re.finditer(r"\bDLLLOCAL\b", text):
        if re.search(r"\b(class|struct|enum)\s*$", text[max(0, match.start() - 8):match.start()]):
            continue
        depth = 0
        position = match.end()
        terminator = None
        while position < len(text):
            character = text[position]
            if character == "(":
                depth += 1
            elif character == ")":
                depth -= 1
            elif depth == 0 and character in ";{":
                terminator = character
                break
            position += 1
        declaration = " ".join(text[match.end():position].split())
        if terminator != ";" or "(" not in declaration:
            continue
        if re.search(r"\binline\b", declaration) or re.search(r"=\s*(default|delete|0)\b", declaration) \
                or re.match(r"(typedef|using)\b", declaration):
            continue
        owner = ""
        for start, end, name in spans:
            if start <= match.start() <= end:
                owner = name
        found.append((owner, declaration))
    return found


def current_entries():
    """The keys of the declarations in the public headers: 'header|class|declaration'"""
    keys = []
    for path in public_headers():
        relative = path.relative_to(ROOT).as_posix()
        for owner, declaration in declarations(path):
            keys.append("%s|%s|%s" % (relative, owner, declaration))
    return keys


def allowlist_entries():
    """The keys of the allowlist, each with an optional reason after ' # '"""
    entries = {}
    for number, line in enumerate(ALLOWLIST.read_text().splitlines(), 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        key, _, reason = line.partition(" # ")
        entries.setdefault(key.rstrip(), []).append((number, reason.strip()))
    return entries


class PublicHeaderVisibilityTest(unittest.TestCase):
    def test_no_new_hidden_declaration(self):
        """A header of the module API does not declare a function that the library does not export"""
        allowed = allowlist_entries()
        new = sorted(key for key in current_entries() if key not in allowed)
        self.assertEqual([], new, "a non-inline DLLLOCAL function is declared in a public header: move the "
            "declaration to the private implementation (include/qore/intern/), make the function inline, or export it "
            "(remove DLLLOCAL and record it in the module API); a declaration that must stay hidden is listed in %s "
            "with its reason" % ALLOWLIST.name)

    def test_allowlist_only_shrinks(self):
        """Every allowlist entry is still declared: an entry whose declaration is gone is removed"""
        current = set(current_entries())
        stale = sorted(key for key in allowlist_entries() if key not in current)
        self.assertEqual([], stale, "the allowlist has entries that no public header declares any more: remove them")

    def test_allowlist_has_no_duplicates(self):
        """An entry is listed once"""
        duplicates = sorted(key for key, lines in allowlist_entries().items() if len(lines) > 1)
        self.assertEqual([], duplicates)

    def test_scanner_finds_what_it_must(self):
        """The scanner treats the forms of a declaration as the rule says"""
        import tempfile

        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / "x.h"
            header.write_text("""class X {
public:
    DLLLOCAL void hidden(int a);
    DLLLOCAL void inlined() { }
    DLLLOCAL inline void inlined2();
    DLLLOCAL virtual void pure() = 0;
    DLLLOCAL X(const X&) = delete;
    // DLLLOCAL void in_a_comment();
    DLLEXPORT void exported();
    DLLLOCAL static int counter;
    DLLLOCAL static void helper();
};
DLLLOCAL void free_function(const char* s);
""")
            self.assertEqual([("X", "void hidden(int a)"), ("X", "static void helper()"),
                              ("", "void free_function(const char* s)")], declarations(header))


if __name__ == "__main__":
    unittest.main()
