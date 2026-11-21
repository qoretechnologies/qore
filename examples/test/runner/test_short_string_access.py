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
an inline string).  The test flags every read as a string node: QoreValue::get<QoreStringNode>() (also split across
lines) and every static_cast, reinterpret_cast or C-style cast to a QoreStringNode pointer, of any expression, also
through a typedef or using alias of the type found in the sources.  A read is accepted only when:
- the code before it in the same function excludes an inline string for the same value, or for the value that it is
  read from (QoreValue::isShortString(), hasNode(), isPlainPointer(), isPointer(), or ensureUnique()/ensure_unique(),
  which make a node of it): a guard of another value does not count;
- a comment just before it says why it is safe ("// note: safe; <reason>"), as for a cast of a node pointer that is
  not read from a value; or
- the allowlist (short-string-access-allowlist.txt) names it with a reason.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[3]
ALLOWLIST = Path(__file__).resolve().parent / "short-string-access-allowlist.txt"
SOURCE_DIRS = ("lib", "include")
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hh", ".hpp", ".qpp", ".ypp", ".lpp", ".in"}

#! the type names of a string node, and of a pointer to one; aliases found in the sources are added (find_aliases())
NODE_TYPES = ("QoreStringNode",)
NODE_POINTER_TYPES = ()
#! a guard call on a value: the value is not an inline short string after it, or is a node
GUARD_CALL = re.compile(r"(?:\.|->)\s*(?:isShortString|hasNode|isPlainPointer|isPointer|ensureUnique)\s*\(")
#! ensure_unique(<value>, ...): makes a node of an inline short string
ENSURE_UNIQUE = re.compile(r"\bensure_unique\s*\(\s*([^,()]+?)\s*[,)]")
#! the comment that says why a read is safe
NOTE = re.compile(r"//.*\bnote: safe;|/\*.*\bnote: safe;")
#! the lines before a read that are searched for a guard
GUARD_WINDOW = 30
#! the lines before a read that are searched for a note
NOTE_WINDOW = 4
#! a line that starts a definition or ends one at the top level: the search for a guard stops there
BOUNDARY = re.compile(r"^(?:\}|[A-Za-z_].*\)\s*(?:const\s*)?(?:\[[^\]]*\]\s*)?\{\s*$)")
#! an alias of the string node type or of a pointer to it
ALIAS = re.compile(r"\btypedef\s+(?:const\s+)?QoreStringNode\s*(\*?)\s*(\w+)\s*;"
    r"|\busing\s+(\w+)\s*=\s*(?:const\s+)?QoreStringNode\s*(\*?)\s*;")


def find_aliases(code):
    """Returns the aliases of the string node type and of a pointer to it in the code, as (types, pointer_types)"""
    types = []
    pointers = []
    for m in ALIAS.finditer(code):
        star, name = (m.group(1), m.group(2)) if m.group(2) else (m.group(4), m.group(3))
        (pointers if star else types).append(name)
    return types, pointers


def access_patterns(types, pointers):
    """Returns the regular expressions of a read of a value as a string node, which can span lines: a get<>() of the
    node type (group "get"), a static_cast or reinterpret_cast to a node pointer (group "cast") and a C-style cast to
    one (group "ccast")"""
    t = "|".join(re.escape(x) for x in types)
    pt = "|".join(re.escape(x) for x in pointers)
    node_ptr = r"(?:const\s+)?(?:%s)\s*\*" % t + (r"|(?:const\s+)?(?:%s)" % pt if pt else "")
    node_arg = r"(?:const\s+)?(?:%s)\s*\*?" % t + (r"|(?:const\s+)?(?:%s)" % pt if pt else "")
    return re.compile(r"(?P<get>(?:\.|->)\s*get\s*<\s*(?:%s)\s*>\s*\(\s*\))" % node_arg
        + r"|(?P<cast>\b(?:static|reinterpret)_cast\s*<\s*(?:%s)\s*>\s*\()" % node_ptr
        + r"|(?P<ccast>\(\s*(?:%s)\s*\)\s*(?=[\w(*&]))" % node_ptr)


def skip_back_balanced(code, i):
    """Returns the index of the bracket that opens the one that closes at code[i]"""
    close = code[i]
    open_ = "(" if close == ")" else "["
    depth = 0
    while i >= 0:
        if code[i] == close:
            depth += 1
        elif code[i] == open_:
            depth -= 1
            if not depth:
                return i
        i -= 1
    return 0


def receiver_before(code, end):
    """Returns the expression that ends just before code[end] (the receiver of a member call), without white space"""
    i = end - 1
    start = end
    while i >= 0:
        while i >= 0 and code[i].isspace():
            i -= 1
        if i < 0:
            break
        if code[i] in ")]":
            i = skip_back_balanced(code, i) - 1
            start = i + 1
            continue
        if code[i].isalnum() or code[i] == "_":
            while i >= 0 and (code[i].isalnum() or code[i] == "_"):
                i -= 1
            start = i + 1
            # a member access, a scope or a dereference continues the expression
            j = i
            while j >= 0 and code[j].isspace():
                j -= 1
            if j >= 1 and code[j - 1:j + 1] in ("->", "::"):
                i = j - 2
                continue
            if j >= 0 and code[j] == ".":
                i = j - 1
                continue
            break
        break
    return "".join(code[start:end].split())


def argument_after(code, i):
    """Returns the expression in the parentheses that open at code[i], without white space"""
    depth = 0
    j = i
    while j < len(code):
        if code[j] == "(":
            depth += 1
        elif code[j] == ")":
            depth -= 1
            if not depth:
                break
        j += 1
    return "".join(code[i + 1:j].split())


def operand_after(code, i):
    """Returns the operand of a C-style cast that starts at code[i], without white space"""
    j = i
    while j < len(code) and code[j].isspace():
        j += 1
    start = j
    while j < len(code):
        if code[j] in "([":
            depth = 0
            close = ")" if code[j] == "(" else "]"
            opener = code[j]
            while j < len(code):
                if code[j] == opener:
                    depth += 1
                elif code[j] == close:
                    depth -= 1
                    if not depth:
                        break
                j += 1
            j += 1
            continue
        if code[j].isalnum() or code[j] in "_*&":
            j += 1
            continue
        if code.startswith("->", j) or code.startswith("::", j) or code[j] == ".":
            j += 2 if code[j] in "-:" else 1
            continue
        break
    return "".join(code[start:j].split())


def same_value(subject, guarded):
    """True if the guard's value is the subject, or the value that the subject is read from"""
    if not guarded:
        return False
    if subject == guarded:
        return True
    return any(subject.startswith(guarded + sep) for sep in (".", "->", "["))


def find_unguarded(text, types=NODE_TYPES, pointers=NODE_POINTER_TYPES):
    """Returns the reads of a value as a string node in the source text that nothing guards, as (line, code)"""
    code = strip_comments_and_strings(text)
    alias_types, alias_pointers = find_aliases(code)
    pattern = access_patterns(tuple(types) + tuple(alias_types), tuple(pointers) + tuple(alias_pointers))
    code_lines = code.split("\n")
    text_lines = text.split("\n")
    line_starts = [0]
    for line in code_lines[:-1]:
        line_starts.append(line_starts[-1] + len(line) + 1)
    found = []
    reported = set()
    for match in pattern.finditer(code):
        # the line of the read: of "get" or of the cast
        pos = match.start() if not match.group("get") else match.start() + match.group("get").index("get")
        index = code.count("\n", 0, pos)
        if match.group("get"):
            subject = receiver_before(code, match.start())
        elif match.group("cast"):
            subject = argument_after(code, match.end() - 1)
        else:
            subject = operand_after(code, match.end())
        # the code before the read in the same function, back to GUARD_WINDOW lines
        first = index
        while first > 0 and index - first < GUARD_WINDOW and not BOUNDARY.match(code_lines[first - 1]):
            first -= 1
        before = code[line_starts[first]:match.start()]
        guarded = any(same_value(subject, receiver_before(before, m.start())) for m in GUARD_CALL.finditer(before)) \
            or any(same_value(subject, "".join(m.group(1).split())) for m in ENSURE_UNIQUE.finditer(before))
        if guarded:
            continue
        if any(NOTE.search(text_lines[prev]) for prev in range(max(0, index - NOTE_WINDOW), index + 1)):
            continue
        if index in reported:
            continue
        reported.add(index)
        found.append((index + 1, normalize(text_lines[index])))
    return found


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

    def test_a_read_split_across_lines_is_found(self):
        # (a) get<>() split across lines
        text = ("size_t f(const QoreValue& v) {\n"
            "    return v.get<\n"
            "        const QoreStringNode>()->size();\n"
            "}\n"
            "size_t g(const QoreValue& v) {\n"
            "    return v\n"
            "        .get <const QoreStringNode\n"
            "        > ()->size();\n"
            "}\n")
        self.assertEqual([2, 7], [line for line, _ in find_unguarded(text)])

    def test_a_node_cast_later_is_found(self):
        # (b) a node fetched into a variable and cast later, with any cast of any expression
        text = ("size_t f(const QoreValue& v) {\n"
            "    const AbstractQoreNode* n = v.getInternalNode();\n"
            "    return static_cast<const QoreStringNode*>(n)->size();\n"
            "}\n"
            "size_t g(const QoreValue& v) {\n"
            "    return reinterpret_cast<QoreStringNode *>(v.getInternalNode())->size();\n"
            "}\n")
        self.assertEqual([3, 6], [line for line, _ in find_unguarded(text)])

    def test_c_style_casts_are_found(self):
        # (c) C-style casts
        text = ("size_t f(const AbstractQoreNode* n) {\n"
            "    return ((QoreStringNode*)n)->size();\n"
            "}\n"
            "size_t g(const QoreValue& v) {\n"
            "    return ((const QoreStringNode *) v.getInternalNode())->size();\n"
            "}\n")
        self.assertEqual([2, 5], [line for line, _ in find_unguarded(text)])

    def test_a_guard_of_another_value_is_not_accepted(self):
        # (d) the guard must be of the value that is read, or of the value it is read from
        text = ("size_t f(const QoreValue& a, const QoreValue& b) {\n"
            "    if (a.isShortString()) {\n"
            "        return 0;\n"
            "    }\n"
            "    ensure_unique(c, nullptr);\n"
            "    return b.get<const QoreStringNode>()->size();\n"
            "}\n"
            "size_t g(LValueHelper& lvh, LValueHelper& other) {\n"
            "    other.ensureUnique();\n"
            "    return lvh.getValue().get<QoreStringNode>()->size();\n"
            "}\n"
            "size_t h(const QoreValue& v, const QoreValue& vv) {\n"
            "    if (vv.hasNode()) {\n"
            "        return v.get<QoreStringNode>()->size();\n"
            "    }\n"
            "    return 0;\n"
            "}\n")
        self.assertEqual([6, 10, 14], [line for line, _ in find_unguarded(text)])

    def test_aliases_are_found(self):
        # (e) aliases of the node type and of a pointer to it
        text = ("typedef QoreStringNode StrNode;\n"
            "using StrNodePtr = const QoreStringNode*;\n"
            "size_t f(const QoreValue& v) {\n"
            "    return v.get<const StrNode>()->size();\n"
            "}\n"
            "size_t g(const QoreValue& v) {\n"
            "    return static_cast<StrNodePtr>(v.getInternalNode())->size();\n"
            "}\n"
            "size_t h(const QoreValue& v) {\n"
            "    return ((StrNode*)v.getInternalNode())->size();\n"
            "}\n")
        self.assertEqual([4, 7, 10], [line for line, _ in find_unguarded(text)])

    def test_guards_of_the_same_value_are_accepted(self):
        # the guard of the value read, or of the value that it is read from, in any form
        text = ("size_t f(LValueHelper& lvh) {\n"
            "    if (lvh.ensureUnique(\"<op>\")) {\n"
            "        return 0;\n"
            "    }\n"
            "    return lvh.getValue().get<QoreStringNode>()->size();\n"
            "}\n"
            "size_t g(const QoreValue& arg) {\n"
            "    if (arg.hasNode() && arg.getInternalNode()->getType() == NT_STRING) {\n"
            "        return static_cast<const QoreStringNode*>(arg.getInternalNode())->size();\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "size_t h(const QoreValue& left) {\n"
            "    return left.isPlainPointer() ? left.get<QoreStringNode>()->size() : 0;\n"
            "}\n"
            "size_t k(QoreValue* p) {\n"
            "    if (p->isShortString()) {\n"
            "        return p->shortStringLen();\n"
            "    }\n"
            "    return p->get<const QoreStringNode>()->size();\n"
            "}\n"
            "QoreStringNode* m(QoreStringNode* s) {\n"
            "    QoreStringNode* (*fp)(QoreStringNode*) = nullptr;\n"
            "    return const_cast<QoreStringNode*>(s);\n"
            "}\n")
        self.assertEqual([], find_unguarded(text))

    def test_comments_and_strings_are_ignored(self):
        text = ("// v.get<QoreStringNode>()->c_str()\n"
            "const char* s = \"v.get<QoreStringNode>()\";\n"
            "/* static_cast<QoreStringNode*>(v.getInternalNode()) */\n")
        self.assertEqual([], find_unguarded(text))


if __name__ == "__main__":
    unittest.main()
