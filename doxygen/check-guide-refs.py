#!/usr/bin/env python3
"""Reject references to navigation bookmarks left behind when guides become pages.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import argparse
from pathlib import Path
import re
import subprocess
import sys


PAGE = re.compile(r"[@\\]page\s+([\w.-]+)")
NAVIGATION = re.compile(r"((?:[@\\]anchor\s+[\w.-]+\s+)+)[@\\](?:subpage|ref)\s+([\w.-]+)")
ANCHOR = re.compile(r"[@\\]anchor\s+([\w.-]+)")
REFERENCE = re.compile(r"[@\\](?:ref|subpage|see|sa)\s+([\w:./#-]+)")
LITERAL = re.compile(r"[@\\](code|verbatim)\b.*?[@\\]end\1\b", re.DOTALL)
SOURCE_DIRS = {"docs", "doxygen", "modules", "qlib", "src", "include", "lib"}
SOURCE_SUFFIXES = {".tmpl", ".dox", ".qpp", ".qm", ".qc", ".h", ".cpp", ".java", ".kt", ".hpp", ".cc", ".c"}


def check_sources(sources):
    """Return (path, line, obsolete bookmark, guide page) for each stale link."""
    prose = {path: LITERAL.sub(lambda m: "\n" * m[0].count("\n"), text)
             for path, text in sources.items()}
    pages = {page for text in prose.values() for page in PAGE.findall(text)}
    aliases = {}
    for text in prose.values():
        for navigation, page in NAVIGATION.findall(text):
            if page in pages:
                aliases.update((anchor, page) for anchor in ANCHOR.findall(navigation) if anchor != page)
    findings = []
    for path, text in prose.items():
        for match in REFERENCE.finditer(text):
            # Doxygen accepts a sentence-ending period immediately after a target.
            target = match[1].rstrip(".")
            if target in aliases:
                findings.append((path, text.count("\n", 0, match.start()) + 1, target, aliases[target]))
    return findings


def repository_sources(repository):
    """Read tracked documentation inputs, excluding untracked build output."""
    result = subprocess.run(["git", "-C", str(repository), "ls-files", "-z"],
                            check=True, capture_output=True, text=True)
    sources = {}
    for name in result.stdout.split("\0"):
        path = Path(name)
        if (not name or path.parts[0] not in SOURCE_DIRS or path.suffix not in SOURCE_SUFFIXES):
            continue
        source = repository / path
        if source.is_file():
            text = source.read_text(encoding="utf-8", errors="replace")
            if "@" in text or "\\" in text:
                sources[source] = text
    return sources


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("repositories", nargs="+", type=Path, help="Qore or binary module Git repositories")
    args = parser.parse_args()
    failures = 0
    for repository in args.repositories:
        try:
            sources = repository_sources(repository)
        except (OSError, subprocess.CalledProcessError) as error:
            parser.error(f"cannot read documentation from {repository}: {error}")
        if not sources:
            parser.error(f"no tracked documentation sources in {repository}")
        for path, line, old, page in check_sources(sources):
            print(f"{path}:{line}: reference to navigation bookmark {old}; use @ref {page}", file=sys.stderr)
            failures += 1
    print(f"Checked {len(args.repositories)} repositories; {failures} obsolete guide references")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
