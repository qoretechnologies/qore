#!/usr/bin/env python3
"""DBI drivers declare their options that can hold credentials sensitive (qore#7070).

A DBI driver registers its options with qore_dbi_method_list::registerOption(); an option whose value can hold a
credential (a password, a key, a passphrase, a token, a Kerberos keytab) or a connection string or URL that can embed
one must be registered with the QDBI_OPT_SENSITIVE flag, so that %Qore masks its value where it shows the options of a
datasource.  These tests read the registerOption() calls of the C++ sources and fail when an option whose name looks
like a secret (see secret_name()) is registered without the flag, or when the name of an option cannot be read.

The flag can be given in the branch of "#ifdef QORE_HAVE_DBI_SENSITIVE_OPTIONS" of a driver that is also built with
older %Qore libraries: the registration in the "#else" branch (or in "#ifndef QORE_HAVE_DBI_SENSITIVE_OPTIONS") is not
read.

The sources of this repository are checked; the drivers of other repositories (the binary module repositories) are
checked with:

    python3 -I examples/test/runner/test_dbi_sensitive_options.py --check <dir>...

or by naming their directories in QORE_DBI_SENSITIVE_OPTION_DIRS (separated by ':') for the unit tests.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import re
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]

#: the directories of this repository with DBI drivers or their option names
REPO_DIRS = ("include", "lib", "modules", "examples/test/module-cpp-api")

#: the C++ source suffixes read
SUFFIXES = (".cpp", ".cc", ".cxx", ".c", ".h", ".hpp", ".hh", ".qpp")

#: name parts that make an option name look like a secret wherever they are in it
SECRET_PARTS = re.compile(r"pass|secret|token|cred|keytab|pwd|key", re.IGNORECASE)
#: name tokens (separated by '-', '_' or '.') that make an option name look like a connection string or URL
SECRET_TOKENS = {"url", "uri", "dsn", "connstr", "connstring"}
#: whole option names that are connection strings
SECRET_NAMES = {"conn", "connection", "conn-str", "conn-string", "connection-string", "connect-string"}

#: the options whose names look like secrets but that hold none: (path relative to the scanned directory, name);
#: each entry says why
ALLOWLIST = set()

#: the options whose names cannot be read from the source, as they are given in a variable: (relative path,
#: expression)
UNRESOLVED_ALLOWLIST = {
    # the test module registers the option names that a test gives it, none of which is a secret
    ("examples/test/module-cpp-api/datasource-option-setter/dsoptprobe-module.cpp", "opt"),
}

STRING = r'"(?:[^"\\\n]|\\.)*"'
DEFINE = re.compile(r"^\s*#\s*define\s+(\w+)\s+(" + STRING + r")\s*$", re.MULTILINE)
CONST = re.compile(r"\b(\w+)\s*(?:\[\s*\])?\s*=\s*(" + STRING + r")\s*;")
FEATURE = "QORE_HAVE_DBI_SENSITIVE_OPTIONS"


def secret_name(name):
    """Returns True if the option name looks like the name of an option that can hold a credential"""
    low = name.lower()
    if low in SECRET_NAMES or SECRET_PARTS.search(low):
        return True
    return any(t in SECRET_TOKENS for t in re.split(r"[-_.]", low))


def strip_comments(text):
    """Removes the comments of C++ source text, keeping string and character literals and line breaks"""
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("\n" * text.count("\n", i, j))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def drop_legacy_branches(text):
    """Blanks the lines that are compiled only without QORE_HAVE_DBI_SENSITIVE_OPTIONS"""
    out = []
    # each entry: (this block is the feature test, the current branch is the legacy one)
    stack = []
    for line in text.split("\n"):
        d = line.strip()
        m = re.match(r"#\s*(\w+)\s*(.*)", d)
        directive = m.group(1) if m else None
        arg = m.group(2) if m else ""
        if directive in ("if", "ifdef", "ifndef"):
            feature = FEATURE in arg
            if not feature:
                stack.append((False, False))
            elif directive == "ifdef" or (directive == "if" and "!" not in arg):
                stack.append((True, False))
            else:
                stack.append((True, True))
            out.append("")
            continue
        if directive in ("else", "elif") and stack:
            feature, legacy = stack[-1]
            if feature:
                stack[-1] = (True, not legacy)
            out.append("")
            continue
        if directive == "endif" and stack:
            stack.pop()
            out.append("")
            continue
        out.append("" if any(legacy for _, legacy in stack) else line)
    return "\n".join(out)


def split_args(text):
    """Splits the text of the arguments of a call at its top-level commas"""
    args = []
    depth = 0
    cur = []
    i = 0
    while i < len(text):
        c = text[i]
        if c == '"':
            m = re.match(STRING, text[i:])
            s = m.group(0) if m else c
            cur.append(s)
            i += len(s)
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == "," and not depth:
            args.append("".join(cur).strip())
            cur = []
            i += 1
            continue
        cur.append(c)
        i += 1
    if "".join(cur).strip():
        args.append("".join(cur).strip())
    return args


def calls(text):
    """Yields (line number, argument list) for each registerOption() call of the text"""
    for m in re.finditer(r"\bregisterOption\s*\(", text):
        i = m.end()
        depth = 1
        j = i
        while j < len(text) and depth:
            if text[j] == '"':
                s = re.match(STRING, text[j:])
                j += len(s.group(0)) if s else 1
                continue
            if text[j] == "(":
                depth += 1
            elif text[j] == ")":
                depth -= 1
            j += 1
        yield text.count("\n", 0, m.start()) + 1, split_args(text[i:j - 1])


def literal(s):
    """Returns the value of one or more adjacent string literals, or None"""
    parts = re.findall(STRING, s)
    if not parts or re.sub(STRING, "", s).strip():
        return None
    return "".join(p[1:-1] for p in parts)


#: the directories that are not read: build trees, fetched dependencies and generated grammars
SKIP_DIRS = re.compile(r"^(build.*|_deps|node_modules|\..*|grammars|third_party|vendor)$")


def source_files(dirs):
    """Yields (the directory given, the path) for each C++ source of the directories"""
    for d in dirs:
        d = Path(d)
        for top, subdirs, names in os.walk(d):
            subdirs[:] = sorted(s for s in subdirs if not SKIP_DIRS.match(s))
            for n in sorted(names):
                p = Path(top) / n
                if p.suffix in SUFFIXES and p.is_file():
                    yield d, p


def rel_path(base, p):
    """The path of a source relative to this repository if it is in it, or to the directory given otherwise"""
    try:
        return p.resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return p.relative_to(base).as_posix()


def read_constants(files):
    """Returns the string constants (macros and constants) of the sources: name -> value"""
    consts = {}
    for _, p in files:
        text = strip_comments(p.read_text(errors="replace"))
        for rx in (DEFINE, CONST):
            for m in rx.finditer(text):
                consts.setdefault(m.group(1), m.group(2)[1:-1])
    return consts


def check(dirs, const_dirs=()):
    """Returns the problems found in the registerOption() calls of the C++ sources of the directories

    @param dirs the directories to check
    @param const_dirs further directories whose string constants give option names (the Qore headers)
    """
    files = list(source_files(dirs))
    consts = read_constants(files + list(source_files(const_dirs)))
    problems = []
    for base, p in files:
        rel = rel_path(base, p)
        text = drop_legacy_branches(strip_comments(p.read_text(errors="replace")))
        for line, args in calls(text):
            # the declaration and the definition of the method itself
            if not args or args[0].startswith("const ") or args[0] in ("name",) and rel in (
                    "lib/DBI.cpp", "include/qore/DBI.h"):
                continue
            name = literal(args[0])
            if name is None:
                name = consts.get(args[0])
            where = f"{p}:{line}"
            if name is None:
                if (rel, args[0]) not in UNRESOLVED_ALLOWLIST:
                    problems.append(f"{where}: the name of the option registered with {args[0]!r} cannot be read; "
                                    "give it as a string literal or a string constant")
                continue
            sensitive = any("QDBI_OPT_SENSITIVE" in a for a in args[3:])
            if secret_name(name) and not sensitive and (rel, name) not in ALLOWLIST:
                problems.append(f"{where}: option {name!r} looks like it can hold a credential but is registered "
                                "without QDBI_OPT_SENSITIVE")
    return problems


def write(d, rel, text):
    p = Path(d) / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text)


class DbiSensitiveOptionsTest(unittest.TestCase):
    def test_repository(self):
        """the DBI drivers of this repository declare their secret-looking options sensitive"""
        self.assertEqual([], check([ROOT / d for d in REPO_DIRS if (ROOT / d).is_dir()]))

    def test_dbitest_secret_is_read(self):
        """the check reads the sensitive option of the dbitest driver through its macro"""
        files = list(source_files([ROOT / "modules/dbi_test"]))
        self.assertEqual("secret", read_constants(files).get("DBITEST_OPT_SECRET"))

    def test_extra_dirs(self):
        """the directories of QORE_DBI_SENSITIVE_OPTION_DIRS (the binary module repositories)"""
        dirs = [d for d in os.environ.get("QORE_DBI_SENSITIVE_OPTION_DIRS", "").split(":") if d]
        if not dirs:
            self.skipTest("QORE_DBI_SENSITIVE_OPTION_DIRS is not set")
        self.assertEqual([], check(dirs, [ROOT / "include"]))

    def test_detection(self):
        with tempfile.TemporaryDirectory() as d:
            write(d, "src/a.cpp", """
#define OPT_PASS "password"
constexpr const char* OPT_URL = "url";
static void init(qore_dbi_method_list& methods) {
    methods.registerOption(OPT_PASS, "the password", stringTypeInfo);
    methods.registerOption(OPT_URL, "the URL", stringTypeInfo);
    methods.registerOption("conn", "the connection string");
    methods.registerOption("ssl-key-passphrase", "the passphrase", stringTypeInfo, QDBI_OPT_SENSITIVE);
    methods.registerOption("timeout", "a timeout", bigIntTypeInfo);
    // methods.registerOption("token", "commented out");
    methods.registerOption(name_var, "unknown");
    methods.registerOption("proxy" "-url", "adjacent literals", stringTypeInfo);
}
""")
            problems = check([d])
            self.assertEqual(5, len(problems), problems)
            for name in ("'password'", "'url'", "'conn'", "name_var", "'proxy-url'"):
                self.assertTrue(any(name in p for p in problems), name)
            for name in ("ssl-key-passphrase", "timeout", "token"):
                self.assertFalse(any(name in p for p in problems), name)

    def test_feature_branches(self):
        """the registration of the legacy branch of the feature macro is not read"""
        with tempfile.TemporaryDirectory() as d:
            write(d, "src/a.cpp", """
#ifdef QORE_HAVE_DBI_SENSITIVE_OPTIONS
    methods.registerOption("conn", "the connection string", stringTypeInfo, QDBI_OPT_SENSITIVE);
#else
    methods.registerOption("conn", "the connection string", stringTypeInfo);
#endif
#ifndef QORE_HAVE_DBI_SENSITIVE_OPTIONS
    methods.registerOption("url", "the URL", stringTypeInfo);
#else
    methods.registerOption("url", "the URL", stringTypeInfo,
        QDBI_OPT_SENSITIVE);
#endif
#ifdef SOMETHING_ELSE
    methods.registerOption("dsn", "the DSN", stringTypeInfo);
#endif
""")
            problems = check([d])
            self.assertEqual(1, len(problems), problems)
            self.assertIn("'dsn'", problems[0])

    def test_secret_names(self):
        for name in ("password", "pass", "PWD", "ssl-key", "api_token", "credentials", "keytab", "conn", "url",
                     "jdbc-url", "dsn", "client-secret", "trustStorePassword"):
            self.assertTrue(secret_name(name), name)
        for name in ("timeout", "connect-timeout", "conn-timeout", "keepalives", "application_name", "classpath",
                     "lob-max-size", "optimal-numbers", "timezone", "security-level", "collation"):
            self.assertFalse(secret_name(name), name)


def main(argv):
    if len(argv) > 1 and argv[1] == "--check":
        problems = check(argv[2:], [ROOT / "include"])
        for p in problems:
            print(p)
        print(f"{len(problems)} problem(s)")
        return 1 if problems else 0
    unittest.main(argv=argv)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
