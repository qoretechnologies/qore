#!/usr/bin/env python3
"""Check initial-release entries and unversioned bullets in Doxygen sources.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import argparse
from pathlib import Path
import re
import sys

HEADING = re.compile(r'(?m)^[ \t]*[@\\](?:sub)*section\s+(\S+)[^\n]*')
INITIAL = re.compile(r'(?:the\s+)?(?:initial|first)\s+(?:public\s+)?(?:release|version)\b', re.IGNORECASE)


def inspect_source(source):
    issues = []
    # Restrict the check to documentation comments; executable strings and
    # examples outside a documentation block are not release history.
    for comment in re.finditer(r'/\*\*.*?\*/', source, re.DOTALL):
        text = comment.group()[3:-2]
        headings = list(HEADING.finditer(text))
        for index, heading in enumerate(headings):
            end = headings[index + 1].start() if index + 1 < len(headings) else len(text)
            body = text[heading.end():end]
            bullets = list(re.finditer(r'(?m)^([ \t]*)[-*]\s+(.*)', body))
            depth = min((len(bullet.group(1)) for bullet in bullets), default=0)
            initial = any(len(bullet.group(1)) == depth and INITIAL.match(bullet.group(2))
                          for bullet in bullets) or INITIAL.match(body.strip())
            if initial and body.strip() != '- initial release':
                issues.append(f'{heading.group(1)}: initial release must contain only "- initial release"')
            if re.search(r'Release (?:Notes|History)', heading.group(), re.IGNORECASE) and bullets:
                issues.append('release notes contain bullets before the first release section')
        if headings and re.search(r'[@\\]page\s+\S+[^\n]*Release (?:Notes|History)', text, re.IGNORECASE):
            if re.search(r'(?m)^\s*[-*]\s+', text[:headings[0].start()]):
                issues.append('release notes contain bullets before the first release section')
    return issues


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('sources', nargs='+', type=Path)
    args = parser.parse_args()
    failures = 0
    for path in args.sources:
        try:
            issues = inspect_source(path.read_text(encoding='utf-8'))
        except OSError as error:
            issues = [str(error)]
        for issue in issues:
            print(f'{path}: {issue}', file=sys.stderr)
            failures += 1
    print(f'Checked {len(args.sources)} sources; {failures} release-note errors')
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
