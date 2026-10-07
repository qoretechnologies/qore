#!/usr/bin/env python3
"""Find unprocessed Doxygen links and Markdown markup in rendered documentation, including subpages.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import argparse
from html.parser import HTMLParser
from pathlib import Path
import re
import sys


COMMAND = re.compile(r"[@\\](?:ref|subpage)\s+([\w:.#/-]+)(\s+\")?")
MARKDOWN_LINK = re.compile(r'!?\[[^\[\]]+\]\(\s*[^\s)]+(?:\s+"[^"]*")?\s*\)')
INLINE_MARKDOWN = re.compile(r"(?<!`)`[^`\n]+`(?!`)|(?<!\*)\*\*[^*\n]+\*\*(?!\*)")
FENCED_MARKDOWN = re.compile(r"(?<!`)```[A-Za-z0-9_+.-]*(?!`)")
MARKUP = re.compile("|".join((MARKDOWN_LINK.pattern, INLINE_MARKDOWN.pattern, FENCED_MARKDOWN.pattern)))
BLOCK_ELEMENTS = {"address", "article", "aside", "blockquote", "dd", "div", "dl", "dt",
                  "fieldset", "figcaption", "figure", "footer", "form", "h1", "h2", "h3",
                  "h4", "h5", "h6", "header", "hr", "li", "main", "nav", "ol", "p",
                  "section", "table", "tbody", "td", "th", "thead", "tr", "ul"}
VOID_ELEMENTS = {"area", "base", "br", "col", "embed", "hr", "img", "input",
                 "link", "meta", "param", "source", "track", "wbr"}
LITERAL_ELEMENTS = {"code", "pre", "tt", "script", "style"}


class ReferenceChecker(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.stack = []
        self.findings = []
        self.prose = []

    def flush_prose(self):
        # Doxygen auto-links a URL even when it leaves surrounding Markdown
        # untouched, splitting one visible link across multiple HTML text nodes.
        text = ''.join(data for data, _ in self.prose)
        index = offset = local_offset = 0
        line = self.prose[0][1] if self.prose else 0
        for match in MARKUP.finditer(text):
            while offset + len(self.prose[index][0]) <= match.start():
                offset += len(self.prose[index][0])
                index += 1
                local_offset = 0
                line = self.prose[index][1]
            start = match.start() - offset
            line += self.prose[index][0].count("\n", local_offset, start)
            local_offset = start
            self.findings.append((line, ' '.join(match.group().split())))
        self.prose.clear()

    def handle_starttag(self, tag, attrs):
        if tag in BLOCK_ELEMENTS:
            self.flush_prose()
        if tag in VOID_ELEMENTS:
            return
        classes = (dict(attrs).get("class") or "").split()
        parent_literal = bool(self.stack and self.stack[-1][1])
        literal = (tag in LITERAL_ELEMENTS or bool(set(classes) & {"fragment", "line", "tt"})
                   or parent_literal)
        inline_literal = literal and (self.stack[-1][2] if parent_literal else
                                      tag in {"code", "tt"} or (tag == "span" and "tt" in classes))
        if literal and not parent_literal:
            if inline_literal:
                # Ignore the literal's contents but retain surrounding prose:
                # **a <code>field</code> value** is still unprocessed Markdown.
                self.prose.append(("{code}", self.getpos()[0]))
            else:
                self.flush_prose()
        self.stack.append((tag, literal, inline_literal))

    def handle_startendtag(self, tag, attrs):
        self.handle_starttag(tag, attrs)
        if tag not in VOID_ELEMENTS:
            self.handle_endtag(tag)

    def handle_endtag(self, tag):
        if tag in BLOCK_ELEMENTS or (self.stack and self.stack[-1][1] and not self.stack[-1][2]):
            self.flush_prose()
        for index in range(len(self.stack) - 1, -1, -1):
            if self.stack[index][0] == tag:
                del self.stack[index:]
                break

    def handle_data(self, data):
        if self.stack and self.stack[-1][1]:
            return
        self.prose.append((data, self.getpos()[0]))
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
    checker.flush_prose()
    return sorted(checker.findings)


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
        if not COMMAND.search(html) and not any(token in html for token in ("](", "`", "**")):
            continue
        for line, command in check_html(html):
            print(f"{path}:{line}: unprocessed documentation markup: {command}", file=sys.stderr)
            failures += 1
    if not count:
        parser.error("no rendered HTML files found")
    print(f"Checked {count} HTML files; {failures} unprocessed documentation markup occurrences")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
