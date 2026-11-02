#!/usr/bin/env python3
"""Check the structure of the Qore language release notes (doxygen/lang/900_release_notes.dox.tmpl).

Every release section must begin with its "@par Release Summary" and contain nothing else before its first
subsection; from Qore 3.0.0 on, the subsections must be the standard ones in the standard order, and every entry
in them must be a bullet or a "@par" block.  See design/release-notes.md.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import argparse
from pathlib import Path
import re
import sys

DEFAULT_SOURCE = Path(__file__).resolve().parent / 'lang/900_release_notes.dox.tmpl'

# the subsections of a release section, in the order in which they must appear
SUBSECTIONS = (
    'compatibility',
    'new_features',
    'deprecated',
    'known_limitations',
    'updated_modules',
    'new_builtin_modules',
    'new_user_modules',
    'bug_fixes',
)

# the first release whose sections must use the standard subsections and entry forms
STRICT_FROM = (3, 0, 0)

# the maximum number of lines of a list item (without its nested items) or of a paragraph in the sections of a release
# from STRICT_FROM on: an entry summarizes a change and links to its documentation
MAX_ENTRY_LINES = 4

SECTION = re.compile(r'^\s*[@\\]section\s+(qore_\S+)')
SUBSECTION = re.compile(r'^\s*[@\\]subsection\s+(\S+)')
SUMMARY = re.compile(r'^\s*[@\\]par\s+Release Summary\s*$')
PAR = re.compile(r'^\s*[@\\]par\b')
ANCHOR = re.compile(r'^\s*[@\\]anchor\s+\S+\s*$')
BULLET = re.compile(r'^\s*[-*]\s')
VERSION = re.compile(r'^qore_(\d+)_(\d+)(?:_(\d+))?$')
# region commands whose content is not checked for entry forms: "start": "end"
REGIONS = {
    'parblock': 'endparblock',
    'code': 'endcode',
    'verbatim': 'endverbatim',
    'htmlonly': 'endhtmlonly',
    'dot': 'enddot',
    'msc': 'endmsc',
}
REGION_START = re.compile(r'[@\\](' + '|'.join(REGIONS) + r')\b')
# a backslash before a double quote is read by doxygen as an escaped quote, which leaves the quotes of the
# paragraph unbalanced, so that the following markup (such as "</tt>") is taken as quoted text
QUOTE_TRAP = re.compile(r'\\\\"')
# doxygen's \c and @c take a word, and render nothing for a word that starts with one of these characters: the
# word is then plain text after an empty code span
EMPTY_CODE = re.compile(r'[@\\]c[ \t]+[.?(\[]')
# doxygen reads \n in text as a line break and \r as an unknown command, also in a \c "..." literal
ESCAPE_LITERAL = re.compile(r'[@\\]c[ \t]+"[^"\s]*\\[rnt]')


def section_version(name):
    """Returns the version of a release section as a tuple, or None for a legacy section id."""
    match = VERSION.match(name)
    if not match:
        return None
    return tuple(int(part or 0) for part in match.groups())


def indent(line):
    return len(line) - len(line.lstrip(' \t'))


def split_sections(lines):
    """Returns a list of (name, start line index, end line index) for each release section."""
    starts = [(index, SECTION.match(line).group(1)) for index, line in enumerate(lines) if SECTION.match(line)]
    end = len(lines)
    for index, line in enumerate(lines):
        if line.strip() == '*/':
            end = index
    sections = []
    for pos, (start, name) in enumerate(starts):
        stop = starts[pos + 1][0] if pos + 1 < len(starts) else end
        sections.append((name, start, stop))
    return sections


def skip_region(lines, index, stop, issues):
    """Returns the index of the line after the region starting on line index; reports a region not ended by stop."""
    match = REGION_START.search(lines[index])
    end_cmd = REGIONS[match.group(1)]
    end = re.compile(r'[@\\]' + end_cmd + r'\b')
    if end.search(lines[index], match.end()):
        return index + 1
    for pos in range(index + 1, stop):
        if end.search(lines[pos]):
            return pos + 1
    issues.append((index + 1, f'@{match.group(1)} without @{end_cmd} in its section or subsection'))
    return stop


def check_preamble(name, lines, start, stop, issues):
    """Checks that a section begins with its release summary, and returns the index of its first subsection."""
    first_sub = next((pos for pos in range(start + 1, stop) if SUBSECTION.match(lines[pos])), stop)
    pos = start + 1
    while pos < first_sub and not lines[pos].strip():
        pos += 1
    if pos >= first_sub or not SUMMARY.match(lines[pos]):
        issues.append((pos + 1, f'{name}: the section must begin with "@par Release Summary"; release notes go in '
                       'the summary or in a subsection, never before the summary'))
        return first_sub
    pos += 1
    while pos < first_sub and not lines[pos].strip():
        pos += 1
    if pos < first_sub and re.match(r'^\s*[@\\]parblock\b', lines[pos]):
        pos = skip_region(lines, pos, first_sub, issues)
    else:
        # a summary without @parblock is one paragraph
        while pos < first_sub and lines[pos].strip():
            pos += 1
    for extra in range(pos, first_sub):
        if lines[extra].strip():
            issues.append((extra + 1, f'{name}: text after the release summary and before the first subsection; '
                           'put it in the summary\'s @parblock or in a subsection'))
            break
    return first_sub


def check_subsections(name, lines, first_sub, stop, issues):
    """Checks the subsection ids and order and the form of each entry of a section."""
    last = -1
    pos = first_sub
    while pos < stop:
        match = SUBSECTION.match(lines[pos])
        if not match:
            pos += 1
            continue
        sub_id = match.group(1)
        suffix = sub_id[len(name) + 1:] if sub_id.startswith(name + '_') else None
        if suffix not in SUBSECTIONS:
            issues.append((pos + 1, f'{name}: subsection "{sub_id}" is not one of the standard subsections: '
                           + ', '.join(f'{name}_{sub}' for sub in SUBSECTIONS)))
        else:
            order = SUBSECTIONS.index(suffix)
            if order <= last:
                issues.append((pos + 1, f'{name}: subsection "{sub_id}" is out of order or repeated; the order is: '
                               + ', '.join(SUBSECTIONS)))
            last = max(last, order)
        sub_stop = next((p for p in range(pos + 1, stop) if SUBSECTION.match(lines[p])), stop)
        check_entries(name, sub_id, lines, pos + 1, sub_stop, issues)
        pos = sub_stop


def check_entries(name, sub_id, lines, start, stop, issues):
    """Checks that every paragraph of a subsection is a bullet entry or a @par block."""
    base = None
    pos = start
    while pos < stop:
        line = lines[pos]
        if not line.strip():
            pos += 1
            continue
        if REGION_START.search(line) and not PAR.match(line):
            pos = skip_region(lines, pos, stop, issues)
            continue
        if base is None:
            base = indent(line)
        para_start = pos
        # a paragraph is a run of non-blank lines; regions inside it are skipped as a whole
        while pos < stop and lines[pos].strip():
            if REGION_START.search(lines[pos]):
                pos = skip_region(lines, pos, stop, issues)
            else:
                pos += 1
        first = lines[para_start]
        # anchors label the entry that follows them
        anchored = para_start
        while anchored + 1 < pos and ANCHOR.match(lines[anchored]):
            anchored += 1
        if ANCHOR.match(first):
            first = lines[anchored]
        if BULLET.match(first) or PAR.match(first) or indent(first) > base:
            continue
        issues.append((para_start + 1, f'{name}: {sub_id}: loose paragraph; every entry must be a bullet ("- ...") '
                       'or a "@par Title" block'))


def check_lengths(name, lines, start, stop, issues):
    """Checks that no list item or paragraph of a section is longer than MAX_ENTRY_LINES lines."""
    pos = start + 1
    while pos < stop:
        line = lines[pos]
        if not line.strip() or (line.lstrip().startswith(('@', '\\')) and not BULLET.match(line)):
            if REGION_START.search(line) and not re.match(r'\s*[@\\]parblock\b', line):
                pos = skip_region(lines, pos, stop, [])
            else:
                pos += 1
            continue
        first = pos
        bullet = BULLET.match(line)
        base = indent(line)
        pos += 1
        # the text of an item ends at a blank line, a command, or a nested or following item
        while pos < stop and lines[pos].strip() and not BULLET.match(lines[pos]) \
                and not lines[pos].lstrip().startswith(('@', '\\')) and (not bullet or indent(lines[pos]) > base):
            pos += 1
        if pos - first > MAX_ENTRY_LINES:
            what = 'list item' if bullet else 'paragraph'
            issues.append((first + 1, f'{name}: {what} of {pos - first} lines; an entry has at most {MAX_ENTRY_LINES} '
                           'lines: summarize the change and link (@ref) to its documentation'))


def inspect_source(source):
    """Returns a list of (line number, message) for each structural problem of the release notes in source."""
    lines = source.split('\n')
    issues = []
    for number, line in enumerate(lines, 1):
        if QUOTE_TRAP.search(line):
            issues.append((number, 'a backslash before a double quote (\\\\") unbalances doxygen\'s quotes and breaks '
                           'the markup that follows; rephrase or use &quot;'))
        if ESCAPE_LITERAL.search(line):
            issues.append((number, 'a \\c "..." literal with a \\r, \\n or \\t escape renders wrongly (doxygen reads \\n as '
                           'a line break); write <tt>&quot;\\\\r\\\\n&quot;</tt> instead'))
        if EMPTY_CODE.search(line):
            issues.append((number, '\\c before a word starting with ".", "?", "(" or "[" renders an empty code span; '
                           'use <tt>...</tt>'))
    sections = split_sections(lines)
    if not sections:
        issues.append((1, 'no release sections found'))
    for name, start, stop in sections:
        first_sub = check_preamble(name, lines, start, stop, issues)
        version = section_version(name)
        if version is not None and version >= STRICT_FROM:
            check_subsections(name, lines, first_sub, stop, issues)
            check_lengths(name, lines, start, stop, issues)
    return sorted(issues)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('sources', nargs='*', type=Path, default=[DEFAULT_SOURCE],
                        help='release notes template(s) to check (default: %(default)s)')
    args = parser.parse_args()
    failures = 0
    for path in args.sources:
        try:
            issues = inspect_source(path.read_text(encoding='utf-8'))
        except OSError as error:
            issues = [(0, str(error))]
        for number, issue in issues:
            print(f'{path}:{number}: {issue}', file=sys.stderr)
            failures += 1
    print(f'Checked {len(args.sources)} release notes sources; {failures} release-note structure errors')
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
