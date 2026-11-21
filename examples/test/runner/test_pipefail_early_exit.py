#!/usr/bin/env python3
"""A shell script of the repository that sets pipefail pipes no output into a reader that exits early (issue #7001).

Under ``set -o pipefail``, ``cmd | grep -q pattern`` fails when ``grep -q`` exits at its first match before ``cmd`` has
written all of its output: ``cmd`` gets SIGPIPE (``echo: write error: Broken pipe``), and the pipeline fails although
the pattern was found.  It does so whenever the output is larger than the pipe's buffer, and under load also with less
(AOTExampleScripts' qo_batch_inherit_test.sh failed so).  The same holds for ``grep -m``, ``grep -l``, ``head``,
``sed q`` and ``awk`` with ``exit``.  Use instead:

- ``grep -q -- pattern <<<"$output"`` (the shell writes the here-string before grep reads it), or
  ``[[ $output == *text* ]]``, for a value that the script holds;
- ``cmd | grep pattern >/dev/null`` (grep reads all of its input) for the output of a command;
- ``cmd | sed -n 1p`` instead of ``cmd | head -1``, and ``awk`` that sets a flag instead of calling ``exit``.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
PIPEFAIL = re.compile(r"\bpipefail\b")
# a pipe (not "||"; "|&" included) into a reader that can exit before its input ends: grep with -q, -m, -l or -L in any
# of its options (or their long forms), head, sed with a q command, and awk with exit
EARLY_EXIT_READER = re.compile(
    r"(?<!\|)\|&?(?!\|)\s*(?:command\s+)?(?:"
    r"[ef]?grep\b(?:[^|;&]*?\s)?"
    r"(?:-[a-zA-Z0-9]*[qmlL][a-zA-Z0-9]*\b|--(?:quiet|silent|max-count|files-with(?:out)?-match(?:es)?)\b)"
    r"|head\b"
    r"|sed\s+(?:-n\s+)?['\"]?[0-9]*q['\"]?(?:\s|$)"
    r"|awk\b[^|]*\bexit\b)")
# a comment: a "#" at the start of a word (not one of ${#var}, $# or a word that contains "#")
COMMENT = re.compile(r"(?:^|\s)#.*$")
# a pipe ("|" or "|&", not "||") at the end of a line
PIPE_AT_END = re.compile(r"(?<!\|)\|&?$")
# directories that hold no tracked script: version control, build directories, dependencies
SKIP_DIRS = re.compile(r"^(?:\..*|build.*|_deps|node_modules|__pycache__)$")


def script_paths(root):
    """Returns the shell scripts of the repository under root"""
    paths = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(d for d in dirnames if not SKIP_DIRS.match(d))
        for name in sorted(filenames):
            if name.endswith((".sh", ".bash")):
                paths.append(Path(dirpath) / name)
    return paths


def logical_lines(text):
    """Returns the lines of a script without comments, with each pipeline that continues on the next line (after a
    "\\", or a "|" or "|&" at the end of a line, which blank lines and comments may follow) joined into one, as
    (number of its first line, text)"""
    lines = []
    start = None
    # True after a line that ends with a pipe: the pipeline goes on after any blank lines and comments
    after_pipe = False
    for number, line in enumerate(text.splitlines(), 1):
        line = COMMENT.sub("", line).rstrip()
        if after_pipe and not line:
            continue
        if start is None:
            start, joined = number, ""
        if line.endswith("\\"):
            joined += line[:-1] + " "
            continue
        joined += line
        after_pipe = bool(PIPE_AT_END.search(line))
        if after_pipe:
            joined += " "
            continue
        lines.append((start, joined))
        start = None
    if start is not None:
        lines.append((start, joined))
    return lines


def find_uses(root):
    """Returns the early-exit readers in the pipelines of the shell scripts under root that set pipefail, as
    (path, line, text)"""
    uses = []
    for path in script_paths(root):
        text = path.read_text(errors="replace")
        if not PIPEFAIL.search(text):
            continue
        for number, line in logical_lines(text):
            if EARLY_EXIT_READER.search(line):
                uses.append((path.relative_to(root), number, line.strip()))
    return uses


class PipefailEarlyExitTest(unittest.TestCase):
    def test_no_early_exit_reader_under_pipefail(self):
        uses = find_uses(ROOT)
        self.assertEqual([], uses, "pipes into a reader that exits early in scripts that set pipefail; see this "
            "test's description: " + "; ".join("%s:%d: %s" % use for use in uses))

    def test_detection(self):
        """The patterns that the test finds, and those that it accepts"""
        for line in ('if echo "$out" | grep -q "x"; then', 'nm "$f" | grep -qE "T x"', 'cmd | grep -m1 x',
                'cmd | grep --quiet x', 'cmd | head -1', 'cmd | grep x | head -n 1', 'cmd | grep -E -q x',
                'cmd | grep -e x -q', 'cmd | egrep -q x', 'cmd |& grep -q x', 'cmd | command grep -q x',
                'cmd | grep -l x', 'cmd | grep --max-count=1 x', "cmd | awk '/x/ {print; exit}'", 'cmd | sed q',
                "cmd | sed -n '1q'", 'cmd | head -c 10'):
            self.assertTrue(EARLY_EXIT_READER.search(COMMENT.sub("", line)), line)
        for line in ('if grep -q -- "x" <<<"$out"; then', 'nm "$f" | grep -E "T x" >/dev/null', 'cmd | sed -n 1p',
                "cmd | awk '/x/ && !n {print; n = 1}'", 'grep -q x file', 'a || head -1 f', 'cmd | grep -c x',
                "cmd | sed 's/q/x/'", 'cmd # | grep -q x', 'n=${#out} | true', 'cmd | grep -v x'):
            self.assertFalse(EARLY_EXIT_READER.search(COMMENT.sub("", line)), line)

    def test_continued_lines(self):
        """A pipeline continued on the next line is checked as one line"""
        text = ('a=1\ncmd \\\n    | grep -q x\ncmd |\n    head -1\ncmd || \\\n    true\ncmd # |\nhead -1 f\n'
            'cmd ||\n    head -1 f\ncmd |\n\n    # c\n    head -1\ncmd |&\n\n    grep -q x\n')
        self.assertEqual([(2, True), (4, True), (6, False), (8, False), (9, False), (10, False), (11, False),
            (12, True), (16, True)],
            [(n, bool(EARLY_EXIT_READER.search(l))) for n, l in logical_lines(text)][1:])


if __name__ == "__main__":
    unittest.main()
