#!/usr/bin/env python3
"""Check rendered module navigation and optional intro logos.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import argparse
from html.parser import HTMLParser
from pathlib import Path
import re
import sys


class Element:
    def __init__(self, tag, attrs=()):
        self.tag = tag
        self.attrs = dict(attrs)
        self.children = []

    def elements(self):
        return [child for child in self.children if isinstance(child, Element)]

    def text(self):
        return ''.join(child.text() if isinstance(child, Element) else child
                       for child in self.children).strip()

    def find(self, tag):
        return ([self] if self.tag == tag else []) + [
            found for child in self.elements() for found in child.find(tag)]


class PageParser(HTMLParser):
    VOID = {'area', 'base', 'br', 'col', 'embed', 'hr', 'img', 'input', 'link',
            'meta', 'param', 'source', 'track', 'wbr'}

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.root = Element('document')
        self.stack = [self.root]

    def handle_starttag(self, tag, attrs):
        node = Element(tag, attrs)
        self.stack[-1].children.append(node)
        if tag not in self.VOID:
            self.stack.append(node)

    def handle_startendtag(self, tag, attrs):
        self.stack[-1].children.append(Element(tag, attrs))

    def handle_endtag(self, tag):
        for index in range(len(self.stack) - 1, 0, -1):
            if self.stack[index].tag == tag:
                del self.stack[index:]
                return

    def handle_data(self, data):
        self.stack[-1].children.append(data)


def inspect_mainpage(html, logo=None):
    parser = PageParser()
    parser.feed(html)
    blocks = [node for node in parser.root.find('div')
              if 'textblock' in (node.attrs.get('class') or '').split()]
    if not blocks:
        return ['missing mainpage content']
    nodes = blocks[0].elements()
    headings = [i for i, node in enumerate(nodes) if re.fullmatch(r'h[1-6]', node.tag)]
    issues = []
    intro_nodes = []
    for number, start in enumerate(headings):
        end = headings[number + 1] if number + 1 < len(headings) else len(nodes)
        body = nodes[start + 1:end]
        if any((a.attrs.get('id') or a.attrs.get('name') or '').endswith('intro')
               for a in nodes[start].find('a')):
            intro_nodes = body
        if len(body) == 1 and body[0].tag == 'p':
            links = [a for a in body[0].find('a') if a.attrs.get('href')]
            if len(links) == 1 and body[0].text() == links[0].text() and not body[0].find('img'):
                issues.append(f'redundant heading with a single navigation link: {nodes[start].text()}')
    if logo and not any(image.attrs.get('src') == logo for node in intro_nodes for image in node.find('img')):
        issues.append(f'logo is missing from the intro section: {logo}')
    return issues


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pages', nargs='+', type=Path)
    parser.add_argument('--logo', help='expected logo filename in the intro section')
    args = parser.parse_args()
    failures = 0
    for page in args.pages:
        try:
            issues = inspect_mainpage(page.read_text(encoding='utf-8'), args.logo)
            if args.logo and not (page.parent / args.logo).is_file():
                issues.append(f'missing exported logo: {args.logo}')
        except OSError as error:
            issues = [str(error)]
        for issue in issues:
            print(f'{page}: {issue}', file=sys.stderr)
            failures += 1
    print(f'Checked {len(args.pages)} mainpages; {failures} layout or logo errors')
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
