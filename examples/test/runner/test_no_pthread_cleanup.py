#!/usr/bin/env python3
"""libqore uses no pthread_cleanup_push() or pthread_cleanup_pop() (issue #5718).

On macOS these macros read and write the cleanup stack in the calling thread's pthread_t
(pthread_self()->__cleanup_stack) in the code that uses them, and the system library reuses the pthread_t of a detached
thread that has ended (a background thread) for a new thread.  That reuse is synchronized in the uninstrumented system
library, so ThreadSanitizer reports the macros of a new thread and of an ended background thread as a data race (a
false positive).  No CI job runs libqore under ThreadSanitizer on macOS, so this test keeps the macros out of the
sources of libqore (lib/ and include/): use QoreThreadCleanupGuard (lib/thread.cpp), or another RAII object, instead.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[3]
SOURCE_DIRS = ("lib", "include")
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hh", ".hpp", ".qpp", ".ypp", ".lpp", ".in"}
MACRO = re.compile(r"\bpthread_cleanup_(push|pop)\s*\(")


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


def find_uses(root):
    """Returns the uses of the macros in the sources of libqore under root as (path, line, text)"""
    uses = []
    for source_dir in SOURCE_DIRS:
        for path in sorted((root / source_dir).rglob("*")):
            if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
                continue
            text = path.read_text(errors="replace")
            if "pthread_cleanup_" not in text:
                continue
            code = strip_comments_and_strings(text)
            for number, line in enumerate(code.split("\n"), 1):
                if MACRO.search(line):
                    uses.append((path.relative_to(root), number, text.split("\n")[number - 1].strip()))
    return uses


class NoPthreadCleanupTest(unittest.TestCase):
    def test_libqore_uses_no_pthread_cleanup_macros(self):
        uses = find_uses(ROOT)
        self.assertEqual([], uses, "libqore must not use pthread_cleanup_push()/pthread_cleanup_pop(): on macOS they "
            "write the cleanup stack of the thread's pthread_t, which the system library reuses for a new thread "
            "after a detached thread ends, and ThreadSanitizer reports that as a data race (issue #5718); use "
            "QoreThreadCleanupGuard (lib/thread.cpp) or another RAII object instead:\n"
            + "\n".join("%s:%d: %s" % use for use in uses))

    def test_comments_and_strings_are_ignored(self):
        code = strip_comments_and_strings('// pthread_cleanup_push(f, 0);\n/* pthread_cleanup_pop(1);\n */ '
            'x = "pthread_cleanup_push(";\nchar c = \'\\\'\';\npthread_cleanup_pop(1);\n')
        lines = code.split("\n")
        self.assertEqual(6, len(lines))
        self.assertEqual([4], [i for i, line in enumerate(lines) if MACRO.search(line)])

    def test_a_use_is_found(self):
        code = strip_comments_and_strings("    pthread_cleanup_push(qore_thread_cleanup, nullptr);\n")
        self.assertTrue(MACRO.search(code))


if __name__ == "__main__":
    unittest.main()
