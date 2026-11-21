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
lines) and every static_cast, reinterpret_cast, dynamic_cast or C-style cast to a QoreStringNode pointer or reference,
of any expression, also through a typedef or using alias of the type found in the sources.  A read is accepted only
when:
- a guard before it in the same function excludes an inline string for the same value (or for the value of an
  LValueHelper, getValue(), or the node of a value, getInternalNode(), or for the pointer that the read dereferences):
  a guard of another value, or of a value that the read only derives another value from, does not count.  A test
  (QoreValue::isShortString(), hasNode(), isPlainPointer(), isPointer()) counts only when it decides that the read
  runs: the read is in the branch of an if, while or ?: that it excludes the inline string from, or after an && or ||
  of it, or after an if on it whose branch leaves (return, continue, break, throw, goto) for the inline string, or
  after an assert() that the value is not one; a call that makes a node of it (ensureUnique()/ensure_unique()) counts
  for the reads that follow it in its block;
- a comment just before it says why it is safe ("// note: safe; <reason>"), as for a cast of a node pointer that is
  not read from a value; a note covers the next read only; or
- the allowlist (short-string-access-allowlist.txt) names it with a reason.

The sources of the binary modules in this repository (modules/: their C++ and qpp sources, and the C++ helpers of
their tests) are held to the same rule with the same checks (issue #7062); they take strings from hashes, lists,
objects, exceptions and the return values of Qore code, all of which can hold inline strings.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
ALLOWLIST = Path(__file__).resolve().parent / "short-string-access-allowlist.txt"
SOURCE_DIRS = ("lib", "include")
#! the sources of the binary modules, which are checked with the same rule as libqore (issue #7062)
MODULE_DIR = "modules"
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hh", ".hpp", ".qpp", ".ypp", ".lpp", ".in"}

#! the type names of a string node, and of a pointer to one; aliases found in the sources are added (find_aliases())
NODE_TYPES = ("QoreStringNode",)
NODE_POINTER_TYPES = ()
#! a guard call on a value: the value is not an inline short string after it, or is a node
GUARD_CALL = re.compile(r"(?:\.|->)\s*(?P<name>isShortString|hasNode|isPlainPointer|isPointer|ensureUnique)\s*\(")
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
    node type (group "get"), a static_cast, reinterpret_cast or dynamic_cast to a node pointer or reference (group
    "cast") and a C-style cast to one (group "ccast")"""
    t = "|".join(re.escape(x) for x in types)
    pt = "|".join(re.escape(x) for x in pointers)
    node_ptr = r"(?:const\s+)?(?:%s)\s*[*&]" % t + (r"|(?:const\s+)?(?:%s)" % pt if pt else "")
    node_arg = r"(?:const\s+)?(?:%s)\s*\*?" % t + (r"|(?:const\s+)?(?:%s)" % pt if pt else "")
    return re.compile(r"(?P<get>(?:\.|->)\s*get\s*<\s*(?:%s)\s*>\s*\(\s*\))" % node_arg
        + r"|(?P<cast>\b(?:static|reinterpret|dynamic)_cast\s*<\s*(?:%s)\s*>\s*\()" % node_ptr
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


def receiver_span(code, end):
    """Returns the start of the expression that ends just before code[end] (the receiver of a member call), and the
    expression without white space"""
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
    return start, "".join(code[start:end].split())


def receiver_before(code, end):
    """Returns the expression that ends just before code[end] (the receiver of a member call), without white space"""
    return receiver_span(code, end)[1]


def argument_after(code, i):
    """Returns the expression in the parentheses that open at code[i], without white space"""
    return "".join(code[i + 1:matching_close(code, i)].split())


def operand_after(code, i):
    """Returns the operand of a C-style cast that starts at code[i], without white space"""
    j = i
    while j < len(code) and code[j].isspace():
        j += 1
    start = j
    while j < len(code):
        if code[j] in "([":
            j = matching_close(code, j) + 1
            continue
        if code[j].isalnum() or code[j] in "_*&":
            j += 1
            continue
        if code.startswith("->", j) or code.startswith("::", j) or code[j] == ".":
            j += 2 if code[j] in "-:" else 1
            continue
        break
    return "".join(code[start:j].split())


#! the accessors that give the value that a guard of their receiver covers: the value of an lvalue (LValueHelper
#! ensureUnique() covers getValue()) and the node of a value (hasNode() covers getInternalNode())
EQUIVALENT_ACCESSORS = (".getValue()", "->getValue()", ".getInternalNode()", "->getInternalNode()")


def same_value(subject, guarded):
    """True if the guard's value is the subject, or the subject is the guard's value through one of the
    EQUIVALENT_ACCESSORS, or a dereference of the guarded pointer"""
    if not guarded:
        return False
    if subject.startswith("*"):
        subject = subject[1:]
    return subject == guarded or any(subject == guarded + accessor for accessor in EQUIVALENT_ACCESSORS)


def matching_close(code, i):
    """Returns the index of the bracket that closes the one that opens at code[i], or len(code)"""
    pairs = {"(": ")", "[": "]", "{": "}"}
    stack = []
    j = i
    while j < len(code):
        c = code[j]
        if c in pairs:
            stack.append(pairs[c])
        elif c in ")]}":
            if stack:
                stack.pop()
            if not stack:
                return j
        j += 1
    return len(code)


def enclosing_open(code, pos):
    """Returns the index of the innermost bracket that encloses code[pos], or -1"""
    depth = 0
    i = pos - 1
    while i >= 0:
        c = code[i]
        if c in ")]}":
            depth += 1
        elif c in "([{":
            if not depth:
                return i
            depth -= 1
        i -= 1
    return -1


def word_before(code, i):
    """Returns the word that ends just before code[i], skipping white space"""
    j = i - 1
    while j >= 0 and code[j].isspace():
        j -= 1
    end = j + 1
    while j >= 0 and (code[j].isalnum() or code[j] == "_"):
        j -= 1
    return code[j + 1:end]


def word_at(code, i):
    """Returns the word that starts at code[i] after white space, and its end"""
    while i < len(code) and code[i].isspace():
        i += 1
    j = i
    while j < len(code) and (code[j].isalnum() or code[j] == "_"):
        j += 1
    return code[i:j], j


def statement_end(code, i):
    """Returns the index just after the statement that starts at code[i]"""
    while i < len(code) and code[i].isspace():
        i += 1
    if i >= len(code):
        return i
    if code[i] == "{":
        return matching_close(code, i) + 1
    word, j = word_at(code, i)
    if word in ("if", "while", "for", "switch"):
        k = j
        while k < len(code) and code[k].isspace():
            k += 1
        end = statement_end(code, matching_close(code, k) + 1)
        if word == "if":
            next_word, after = word_at(code, end)
            if next_word == "else":
                end = statement_end(code, after)
        return end
    j = i
    while j < len(code) and code[j] not in ";}":
        j = matching_close(code, j) + 1 if code[j] in "([{" else j + 1
    return j + 1 if j < len(code) and code[j] == ";" else j


def top_level_ops(code, start, end):
    """Returns the operators &&, ||, ? and : at the top level of code[start:end], as (index, op)"""
    ops = []
    i = start
    while i < end:
        c = code[i]
        if c in "([{":
            i = matching_close(code, i) + 1
            continue
        if code.startswith("&&", i) or code.startswith("||", i):
            ops.append((i, code[i:i + 2]))
            i += 2
            continue
        if c == "?":
            ops.append((i, "?"))
        elif c == ":" and not code.startswith("::", i) and (i == 0 or code[i - 1] != ":"):
            ops.append((i, ":"))
        i += 1
    return ops


def expression_region(code, pos):
    """Returns the region of the expression that contains code[pos]: the inside of the parentheses or brackets that
    enclose it, or its statement"""
    open_ = enclosing_open(code, pos)
    if open_ >= 0 and code[open_] in "([":
        return open_ + 1, matching_close(code, open_), open_
    start = pos
    while start > 0 and code[start - 1] not in ";{}":
        start -= 1
        if code[start] in ")]":
            start = skip_back_balanced(code, start)
    end = pos
    while end < len(code) and code[end] not in ";}":
        end = matching_close(code, end) + 1 if code[end] in "([{" else end + 1
    return start, end, -1


EXIT = re.compile(r"\s*(?:return|continue|break|throw|goto)\b")


def ends_with_exit(code, start, end):
    """True if the statement in code[start:end] (a block or a single statement) leaves the code that follows it"""
    i = start
    while i < end and code[i].isspace():
        i += 1
    if i < end and code[i] == "{":
        stmts = []
        j = i + 1
        close = matching_close(code, i)
        while j < close:
            while j < close and code[j].isspace():
                j += 1
            if j >= close:
                break
            k = statement_end(code, j)
            stmts.append(j)
            j = k
        return bool(stmts) and bool(EXIT.match(code, stmts[-1]))
    return bool(EXIT.match(code, i))


def guard_covers(code, guard_start, guard_end, means_short, read):
    """True if the guard term code[guard_start:guard_end], whose truth means that the value is an inline short string
    if means_short (and that it is not one otherwise), excludes an inline string at the read at code[read]: the read
    is in the branch where the term excludes it, or after the code leaves for the inline string"""
    start, end, open_ = expression_region(code, guard_start)
    ops = top_level_ops(code, start, end)
    before = [op for i, op in ops if i < guard_start]
    if start <= read < end:
        # the read is in the same expression: a ternary or a chain of && or ||
        between = [op for i, op in ops if guard_end <= i < read]
        question = [i for i, op in ops if op == "?" and i >= guard_end]
        if question and "?" not in before and ":" not in before:
            q = question[0]
            cond = [op for i, op in ops if i < q]
            nested = 0
            colon = end
            for i, op in ops:
                if i <= q:
                    continue
                if op == "?":
                    nested += 1
                elif op == ":":
                    if not nested:
                        colon = i
                        break
                    nested -= 1
            if q < read < colon:
                return not means_short and "||" not in cond
            if read > colon:
                return means_short and "&&" not in cond
        if between and all(op == "&&" for op in between):
            return not means_short
        if between and all(op == "||" for op in between) and (not before or before[-1] == "||"):
            return means_short
        return False
    if open_ >= 0 and code[open_] == "(" and word_before(code, open_) == "assert":
        # an assertion that the value is not an inline string: the reads after it in its block
        if not means_short and "||" not in [op for i, op in ops] and read > end:
            block = enclosing_open(code, open_)
            return block < 0 or read < matching_close(code, block)
        return False
    if open_ < 0 or code[open_] != "(" or word_before(code, open_) not in ("if", "while"):
        return False
    # the guard is a term of the condition of an if or a while
    cond = [op for i, op in ops]
    if "?" in cond or ":" in cond:
        return False
    body_start = end + 1
    body_end = statement_end(code, body_start)
    if body_start <= read < body_end:
        return not means_short and "||" not in cond
    if word_before(code, open_) != "if":
        return False
    word, after = word_at(code, body_end)
    if word == "else":
        else_end = statement_end(code, after)
        if after <= read < else_end:
            return means_short and "&&" not in cond
    else:
        else_end = body_end
    # the code leaves for the inline string before the read, in the block of the if
    if means_short and "&&" not in cond and read >= else_end and ends_with_exit(code, body_start, body_end):
        block = enclosing_open(code, open_ - 1)
        return block < 0 or read < matching_close(code, block)
    return False


def ensure_covers(code, call, read):
    """True if the call at code[call] that makes a node of an inline string runs before the read at code[read]: the
    read is in the block of the call"""
    block = call
    while True:
        block = enclosing_open(code, block)
        if block < 0 or code[block] == "{":
            break
    return block < 0 or read < matching_close(code, block)


def is_guarded(code, scope_start, read, subject):
    """True if a guard in code[scope_start:read] excludes an inline string at the read of the subject at code[read]"""
    before = code[scope_start:read]
    for m in GUARD_CALL.finditer(before):
        r_start, guarded = receiver_span(before, m.start())
        if not same_value(subject, guarded):
            continue
        call_open = scope_start + m.end() - 1
        call_close = matching_close(code, call_open)
        if m.group("name") == "ensureUnique":
            if ensure_covers(code, scope_start + m.start(), read):
                return True
            continue
        # the term, with the negations before it
        term_start = scope_start + r_start
        negations = 0
        i = term_start - 1
        while i >= 0 and (code[i].isspace() or code[i] == "!"):
            if code[i] == "!":
                negations += 1
            i -= 1
        if i >= 0 and code[i] == "!":
            continue
        term_start = i + 1
        means_short = (m.group("name") == "isShortString") != bool(negations % 2)
        if guard_covers(code, term_start, call_close + 1, means_short, read):
            return True
    for m in ENSURE_UNIQUE.finditer(before):
        if same_value(subject, "".join(m.group(1).split())) and ensure_covers(code, scope_start + m.start(), read):
            return True
    return False


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
    reads = []
    for match in pattern.finditer(code):
        # the line of the read: of "get" or of the cast
        pos = match.start() if not match.group("get") else match.start() + match.group("get").index("get")
        reads.append((match, code.count("\n", 0, pos)))
    # a note covers the next read only, if it is within NOTE_WINDOW lines of it
    covered = set()
    notes = [n for n, line in enumerate(text_lines) if NOTE.search(line)]
    for note in notes:
        following = [k for k, (_, index) in enumerate(reads) if index >= note]
        if following and reads[following[0]][1] - note <= NOTE_WINDOW:
            covered.add(following[0])
    found = []
    reported = set()
    for k, (match, index) in enumerate(reads):
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
        if is_guarded(code, line_starts[first], match.start(), subject):
            continue
        if k in covered:
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


def find_module_sites(root):
    """Returns the unguarded reads in the sources of the binary modules under root (modules/) as (path, line, code);
    the reads are found with find_unguarded(), as in libqore"""
    sites = []
    for path in sorted((root / MODULE_DIR).rglob("*")):
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

    def test_modules_read_string_values_in_both_representations(self):
        allowed = {(path, code) for path, code, _ in read_allowlist(ALLOWLIST)}
        sites = [site for site in find_module_sites(ROOT) if (site[0], site[2]) not in allowed]
        self.assertEqual([], sites, "a binary module reads a string value as a QoreStringNode without excluding an "
            "inline short string, for which getType() returns NT_STRING but get<QoreStringNode>() returns nullptr "
            "(issue #7062); read it with QoreStringDataHelper, or with QoreStringValueHelper or "
            "QoreStringNodeValueHelper if a string object is needed; if an inline string cannot reach the code, say "
            "why in a \"// note: safe; <reason>\" comment just before it, or add it to " + ALLOWLIST.name
            + " with the reason:\n" + "\n".join("%s:%d: %s" % site for site in sites))

    def test_allowlist_entries_are_used(self):
        sites = {(path, code) for path, _, code in find_sites(ROOT) + find_module_sites(ROOT)}
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

    def test_a_guard_must_decide_the_branch_of_the_read(self):
        # a guard counts only when the read is in the branch where it excludes an inline string, or after the code
        # leaves for one
        text = ("size_t f1(const QoreValue& v) {\n"
            "    bool s = v.isShortString();\n"
            "    return v.get<const QoreStringNode>()->size();\n"
            "}\n"
            "size_t f2(const QoreValue& v) {\n"
            "    if (v.isShortString()) {\n"
            "        log();\n"
            "    }\n"
            "    return v.get<const QoreStringNode>()->size();\n"
            "}\n"
            "size_t f3(const QoreValue& v) {\n"
            "    if (v.hasNode()) {\n"
            "        log();\n"
            "    }\n"
            "    return v.get<const QoreStringNode>()->size();\n"
            "}\n"
            "size_t f4(const QoreValue& v, bool x) {\n"
            "    if (v.isShortString() && x) {\n"
            "        return 0;\n"
            "    }\n"
            "    return v.get<const QoreStringNode>()->size();\n"
            "}\n"
            "size_t f5(const QoreValue& v, bool x) {\n"
            "    if (v.hasNode() || x) {\n"
            "        return v.get<const QoreStringNode>()->size();\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "size_t f6(const QoreValue& v) {\n"
            "    return v.isShortString() ? v.get<const QoreStringNode>()->size() : 0;\n"
            "}\n"
            "size_t f7(const QoreValue& v) {\n"
            "    if (v.hasNode()) {\n"
            "        return 0;\n"
            "    }\n"
            "    return v.get<const QoreStringNode>()->size();\n"
            "}\n"
            "size_t f8(const QoreValue& v) {\n"
            "    {\n"
            "        if (v.isShortString()) {\n"
            "            return 0;\n"
            "        }\n"
            "    }\n"
            "    return v.get<const QoreStringNode>()->size();\n"
            "}\n"
            "size_t f9(LValueHelper& lvh, bool x) {\n"
            "    if (x) {\n"
            "        lvh.ensureUnique();\n"
            "    }\n"
            "    return lvh.getValue().get<QoreStringNode>()->size();\n"
            "}\n")
        self.assertEqual([3, 9, 15, 21, 25, 30, 36, 44, 50], [line for line, _ in find_unguarded(text)])

    def test_guards_that_decide_the_branch_are_accepted(self):
        text = ("size_t f1(const QoreValue& v) {\n"
            "    if (v.isShortString()) {\n"
            "        return 0;\n"
            "    } else {\n"
            "        return v.get<const QoreStringNode>()->size();\n"
            "    }\n"
            "}\n"
            "size_t f2(const QoreValue& v) {\n"
            "    if (!v.hasNode())\n"
            "        return 0;\n"
            "    return v.get<const QoreStringNode>()->size();\n"
            "}\n"
            "size_t f3(const QoreValue& v, bool x) {\n"
            "    if (v.isShortString() || x) {\n"
            "        log();\n"
            "        throw 1;\n"
            "    }\n"
            "    return v.get<const QoreStringNode>()->size();\n"
            "}\n"
            "bool f4(const QoreValue& v) {\n"
            "    return v.hasNode() && v.get<const QoreStringNode>()->empty();\n"
            "}\n"
            "bool f5(const QoreValue& v) {\n"
            "    return v.isShortString() || v.get<const QoreStringNode>()->empty();\n"
            "}\n"
            "size_t f6(const QoreValue& v) {\n"
            "    return v.isShortString() ? 0 : v.get<const QoreStringNode>()->size();\n"
            "}\n"
            "void f7(const QoreListNode* l) {\n"
            "    for (size_t i = 0; i < l->size(); ++i) {\n"
            "        QoreValue v = l->retrieveEntry(i);\n"
            "        if (v.isShortString()) {\n"
            "            continue;\n"
            "        }\n"
            "        use(v.get<const QoreStringNode>());\n"
            "    }\n"
            "}\n"
            "size_t f8(const QoreValue& v) {\n"
            "    assert(v.hasNode());\n"
            "    return v.get<const QoreStringNode>()->size();\n"
            "}\n"
            "size_t f9(const QoreValue& v) {\n"
            "    while (v.isPlainPointer()) {\n"
            "        return v.get<const QoreStringNode>()->size();\n"
            "    }\n"
            "    return 0;\n"
            "}\n")
        self.assertEqual([], find_unguarded(text))

    def test_reference_and_dynamic_casts_are_found(self):
        text = ("size_t f(const AbstractQoreNode* n) {\n"
            "    return static_cast<const QoreStringNode&>(*n).size();\n"
            "}\n"
            "size_t g(AbstractQoreNode* n) {\n"
            "    return static_cast<QoreStringNode &>(*n).size();\n"
            "}\n"
            "size_t h(const AbstractQoreNode* n) {\n"
            "    return ((const QoreStringNode&)*n).size();\n"
            "}\n"
            "size_t k(const QoreValue& v) {\n"
            "    auto* s = dynamic_cast<const QoreStringNode*>(v.getInternalNode());\n"
            "    return s ? s->size() : 0;\n"
            "}\n"
            "size_t m(const AbstractQoreNode* n) {\n"
            "    return dynamic_cast<const QoreStringNode&>(*n).size();\n"
            "}\n"
            "size_t p(const QoreValue& v) {\n"
            "    if (v.hasNode()) {\n"
            "        return static_cast<const QoreStringNode&>(*v.getInternalNode()).size();\n"
            "    }\n"
            "    return 0;\n"
            "}\n")
        self.assertEqual([2, 5, 8, 11, 15], [line for line, _ in find_unguarded(text)])

    def test_a_guard_covers_its_own_value_only(self):
        # a guard of a value does not cover a value derived from it, and a note covers the next read only
        text = ("size_t f(const QoreValue& h) {\n"
            "    if (h.isPointer()) {\n"
            "        return h.getX().get<QoreStringNode>()->size();\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "size_t g(const QoreValue* h) {\n"
            "    if (h->hasNode()) {\n"
            "        return h->get<const QoreHashNode>()->getKeyValue(\"a\").get<QoreStringNode>()->size();\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "size_t k(const AbstractQoreNode* a, const AbstractQoreNode* b) {\n"
            "    // note: safe; a is a string node\n"
            "    size_t rv = static_cast<const QoreStringNode*>(a)->size();\n"
            "    return rv + static_cast<const QoreStringNode*>(b)->size();\n"
            "}\n")
        self.assertEqual([3, 9, 16], [line for line, _ in find_unguarded(text)])

    def test_module_sources_are_scanned(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            src = root / MODULE_DIR / "m" / "src"
            src.mkdir(parents=True)
            (src / "m.qpp").write_text("static int f(const QoreValue& v) {\n"
                "    return v.get<const QoreStringNode>()->size();\n"
                "}\n")
            (src / "safe.cpp").write_text("static size_t f(const QoreValue& v) {\n"
                "    return QoreStringDataHelper(v).size();\n"
                "}\n"
                "// QoreStringNode\n")
            (src / "other.txt").write_text("v.get<QoreStringNode>()\n")
            self.assertEqual([("modules/m/src/m.qpp", 2, "return v.get<const QoreStringNode>()->size();")],
                find_module_sites(root))

    def test_comments_and_strings_are_ignored(self):
        text = ("// v.get<QoreStringNode>()->c_str()\n"
            "const char* s = \"v.get<QoreStringNode>()\";\n"
            "/* static_cast<QoreStringNode*>(v.getInternalNode()) */\n")
        self.assertEqual([], find_unguarded(text))


if __name__ == "__main__":
    unittest.main()
