#!/usr/bin/env python3
"""libqore reads string values in both of their representations (issue #7038).

A QoreValue holds a string of up to QoreValue::SHORTSTR_MAX_BYTES (6) UTF-8 bytes inline, with no QoreStringNode
behind it: getType() returns NT_STRING for it, but get<QoreStringNode>() returns nullptr (and a debug build asserts),
so code that reads a string value as a node after checking only its type dereferences a null pointer.  The values that
compiled (qcc), JIT and IR code produce, and with them the values of hashes, lists, objects and exceptions, can be
inline strings, so the representation is not predictable from the code that reads a value.

This test keeps the raw pattern out of the sources of libqore (lib/ and include/): a string value is read with
QoreStringDataHelper (the bytes, size and encoding of either representation, with no allocation), or with
QoreStringValueHelper or QoreStringNodeValueHelper when a QoreString or QoreStringNode is needed (they make a node of
an inline string).  A read of a value as a QoreStringNode, as QoreValue::get<QoreStringNode>() or a cast of
QoreValue::getInternalNode() to QoreStringNode, is accepted only when the code before it in the same function excludes
an inline string (QoreValue::isShortString(), hasNode(), isPlainPointer(), or ensureUnique()/ensure_unique(), which
make a node of it), when a comment just before it says why it is safe ("note: safe; <reason>"), or when the
allowlist (short-string-access-allowlist.txt) names it with a reason.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[3]
ALLOWLIST = Path(__file__).resolve().parent / "short-string-access-allowlist.txt"
SOURCE_DIRS = ("lib", "include")
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hh", ".hpp", ".qpp", ".ypp", ".lpp", ".in"}

#! a read of a value as a string node: get<QoreStringNode>() or a cast of a value's node to QoreStringNode
ACCESS = re.compile(r"\bget\s*<\s*(?:const\s+)?QoreStringNode\s*\*?\s*>\s*\(\s*\)"
    r"|\b(?:static|reinterpret)_cast\s*<\s*(?:const\s+)?QoreStringNode\s*\*\s*>\s*\([^;]*?getInternalNode\s*\(\s*\)")
#! code that excludes an inline short string from the value read after it
GUARD = re.compile(r"\bisShortString\s*\(|\bhasNode\s*\(|\bisPlainPointer\s*\(|\bensureUnique\s*\(|\bensure_unique\s*\(")
#! the comment that says why a read is safe
NOTE = re.compile(r"//.*\bnote: safe;|/\*.*\bnote: safe;")
#! the lines before a read that are searched for a guard
GUARD_WINDOW = 30
#! the lines before a read that are searched for a note
NOTE_WINDOW = 4
#! a line that starts a definition or ends one at the top level: the search for a guard stops there
BOUNDARY = re.compile(r"^(?:\}|[A-Za-z_].*\)\s*(?:const\s*)?(?:\[[^\]]*\]\s*)?\{\s*$)")


def strip_comments_and_strings(text):
    """Returns the C/C++ source with its comments and string and character literals replaced by spaces; line breaks
    are kept, so that line numbers stay the same"""
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def normalize(code):
    """Returns the code of a line with its white space collapsed, as the allowlist gives it"""
    return " ".join(code.split())


def find_unguarded(text):
    """Returns the reads of a value as a string node in the source text that nothing guards, as (line, code)"""
    code_lines = strip_comments_and_strings(text).split("\n")
    text_lines = text.split("\n")
    found = []
    for index, line in enumerate(code_lines):
        for match in ACCESS.finditer(line):
            # a guard earlier on the same line, or in the lines before it in the same function
            if GUARD.search(line[:match.start()]):
                continue
            guarded = False
            for prev in range(index - 1, max(-1, index - GUARD_WINDOW - 1), -1):
                if GUARD.search(code_lines[prev]):
                    guarded = True
                    break
                if BOUNDARY.match(code_lines[prev]):
                    break
            if guarded:
                continue
            if any(NOTE.search(text_lines[prev]) for prev in range(max(0, index - NOTE_WINDOW), index + 1)):
                continue
            found.append((index + 1, normalize(text_lines[index])))
            break
    return found


def find_sites(root):
    """Returns the unguarded reads in the sources of libqore under root as (path, line, code)"""
    sites = []
    for source_dir in SOURCE_DIRS:
        for path in sorted((root / source_dir).rglob("*")):
            if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
                continue
            text = path.read_text(errors="replace")
            if "QoreStringNode" not in text:
                continue
            for line, code in find_unguarded(text):
                sites.append((path.relative_to(root).as_posix(), line, code))
    return sites


def read_allowlist(path):
    """Returns the allowlist entries as (path, code, reason); format: <path> | <code of the line> | <reason>"""
    entries = []
    for number, line in enumerate(path.read_text().split("\n"), 1):
        if not line.strip() or line.startswith("#"):
            continue
        parts = [part.strip() for part in line.split(" | ")]
        if len(parts) != 3 or not all(parts):
            raise ValueError("%s:%d: expecting \"<path> | <code> | <reason>\": %s" % (path, number, line))
        entries.append((parts[0], normalize(parts[1]), parts[2]))
    return entries


class ShortStringAccessTest(unittest.TestCase):
    def test_libqore_reads_string_values_in_both_representations(self):
        allowed = {(path, code) for path, code, _ in read_allowlist(ALLOWLIST)}
        sites = [site for site in find_sites(ROOT) if (site[0], site[2]) not in allowed]
        self.assertEqual([], sites, "a string value is read as a QoreStringNode without excluding an inline short "
            "string, for which getType() returns NT_STRING but get<QoreStringNode>() returns nullptr (issue #7038); "
            "read it with QoreStringDataHelper, or with QoreStringValueHelper or QoreStringNodeValueHelper if a "
            "string object is needed; if an inline string cannot reach the code, say why in a \"// note: safe; "
            "<reason>\" comment just before it, or add it to " + ALLOWLIST.name + " with the reason:\n"
            + "\n".join("%s:%d: %s" % site for site in sites))

    def test_allowlist_entries_are_used(self):
        sites = {(path, code) for path, _, code in find_sites(ROOT)}
        stale = [(path, code) for path, code, _ in read_allowlist(ALLOWLIST) if (path, code) not in sites]
        self.assertEqual([], stale, "these entries of " + ALLOWLIST.name + " name no unguarded read; remove them")

    def test_raw_pattern_is_found(self):
        text = ("int f(const QoreValue& v) {\n"
            "    if (v.getType() == NT_STRING) {\n"
            "        return strlen(v.get<const QoreStringNode>()->c_str());\n"
            "    }\n"
            "    return static_cast<const QoreStringNode*>(v.getInternalNode())->size();\n"
            "}\n")
        self.assertEqual([3, 5], [line for line, _ in find_unguarded(text)])

    def test_guards_and_notes_are_accepted(self):
        text = ("int f(QoreValue& v) {\n"
            "    if (v.isShortString()) {\n"
            "        return v.shortStringLen();\n"
            "    }\n"
            "    return v.get<const QoreStringNode>()->size();\n"
            "}\n"
            "\n"
            "int g(QoreValue& v) {\n"
            "    ensure_unique(v, nullptr);\n"
            "    return v.get<QoreStringNode>()->chomp();\n"
            "}\n"
            "\n"
            "int h(QoreStringNode* n) {\n"
            "    QoreValue v(n);\n"
            "    // note: safe; the value was just made from a node\n"
            "    return v.get<QoreStringNode>()->size();\n"
            "}\n")
        self.assertEqual([], find_unguarded(text))

    def test_a_guard_in_another_function_is_not_accepted(self):
        text = ("bool f(const QoreValue& v) {\n"
            "    return v.isShortString();\n"
            "}\n"
            "\n"
            "size_t g(const QoreValue& v) {\n"
            "    return v.get<const QoreStringNode>()->size();\n"
            "}\n")
        self.assertEqual([6], [line for line, _ in find_unguarded(text)])

    def test_comments_and_strings_are_ignored(self):
        text = ("// v.get<QoreStringNode>()->c_str()\n"
            "const char* s = \"v.get<QoreStringNode>()\";\n"
            "/* static_cast<QoreStringNode*>(v.getInternalNode()) */\n")
        self.assertEqual([], find_unguarded(text))


if __name__ == "__main__":
    unittest.main()
