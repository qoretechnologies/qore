#!/usr/bin/env python3
"""Find unprocessed Doxygen links in rendered documentation, including subpages.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import argparse
from html.parser import HTMLParser
from pathlib import Path
import re
import sys


COMMAND = re.compile(r"[@\\](?:ref|subpage)\s+([\w:.#/-]+)(\s+\")?")
VOID_ELEMENTS = {"area", "base", "br", "col", "embed", "hr", "img", "input",
                 "link", "meta", "param", "source", "track", "wbr"}
LITERAL_ELEMENTS = {"code", "pre", "tt", "script", "style"}


class ReferenceChecker(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.stack = []
        self.findings = []

    def handle_starttag(self, tag, attrs):
        if tag in VOID_ELEMENTS:
            return
        classes = (dict(attrs).get("class") or "").split()
        literal = (tag in LITERAL_ELEMENTS or bool(set(classes) & {"fragment", "line", "tt"})
                   or bool(self.stack and self.stack[-1][1]))
        self.stack.append((tag, literal))

    def handle_startendtag(self, tag, attrs):
        pass

    def handle_endtag(self, tag):
        for index in range(len(self.stack) - 1, -1, -1):
            if self.stack[index][0] == tag:
                del self.stack[index:]
                break

    def handle_data(self, data):
        if self.stack and self.stack[-1][1]:
            return
        for match in COMMAND.finditer(data):
            target, quoted_label = match.groups()
            # Allow prose about the directive itself, e.g. "resolve @ref tags".
            # Links with labels, qualified symbols or module intro targets are unambiguous.
            if quoted_label or "::" in target or target.endswith("intro"):
                line = self.getpos()[0] + data[:match.start()].count("\n")
                self.findings.append((line, match.group().strip()))


def check_html(html):
    checker = ReferenceChecker()
    checker.feed(html)
    checker.close()
    return checker.findings


def html_files(roots):
    """Follow composed-documentation symlinks once, without directory cycles."""
    pending = list(roots)
    seen = set()
    while pending:
        path = pending.pop()
        resolved = path.resolve()
        if resolved in seen:
            continue
        seen.add(resolved)
        if path.is_dir():
            pending.extend(path.iterdir())
        elif path.suffix.lower() == ".html" and path.is_file():
            yield path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", type=Path, nargs="+", help="HTML files or documentation roots")
    args = parser.parse_args()
    for path in args.paths:
        if not path.exists():
            parser.error(f"documentation path does not exist: {path}")
    count = failures = 0
    for path in html_files(args.paths):
        count += 1
        html = path.read_text(encoding="utf-8", errors="replace")
        if not COMMAND.search(html):
            continue
        for line, command in check_html(html):
            print(f"{path}:{line}: unprocessed Doxygen link: {command}", file=sys.stderr)
            failures += 1
    if not count:
        parser.error("no rendered HTML files found")
    print(f"Checked {count} HTML files; {failures} unprocessed Doxygen links")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
