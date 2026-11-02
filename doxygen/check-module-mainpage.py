#!/usr/bin/env python3
"""Check rendered module navigation and optional intro logos.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import argparse
from html.parser import HTMLParser
from pathlib import Path
import re
import sys
from urllib.parse import urlsplit


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


# a module logo has a fixed size in pixels, never a size relative to the page: square logos (up to 2:1) are 96 pixels
# wide, wider wordmarks 48 pixels high (see design/qore-module-structure.md)
LOGO_SIZES = ({'width': '96'}, {'height': '48'})
IMG = re.compile(r'<img\s[^>]*>', re.IGNORECASE)
ATTR = re.compile(r'([A-Za-z-]+)\s*=\s*"([^"]*)"')


def logo_size_issue(attrs):
    """Returns an error message if a logo image does not have one of the standard sizes, or None."""
    size = {key: attrs[key] for key in ('width', 'height') if key in attrs}
    style = attrs.get('style') or ''
    if re.search(r'\b(width|height)\s*:', style) or size not in LOGO_SIZES:
        shown = ' '.join(f'{key}="{value}"' for key, value in sorted(attrs.items()) if key in ('width', 'height', 'style'))
        return (f'logo {attrs.get("src", "")} has the size [{shown or "none"}]; a logo has width="96", or height="48" '
                'for a wordmark wider than 2:1, and no style size')
    return None


def inspect_logo_sources(text):
    """Returns a list of (line number, message) for each logo image of documentation source text with another size."""
    issues = []
    for match in IMG.finditer(text):
        attrs = {key.lower(): value for key, value in ATTR.findall(match.group())}
        if 'logo' not in (attrs.get('src', '') + attrs.get('alt', '')).lower() or attrs.get('src', '').startswith('cid:'):
            continue
        issue = logo_size_issue(attrs)
        if issue:
            issues.append((text.count('\n', 0, match.start()) + 1, issue))
    return issues


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
    guide_anchors = set()
    for item in blocks[0].find('li'):
        children = item.elements()
        for anchor, link in zip(children, children[1:]):
            if anchor.tag == link.tag == 'a' and anchor.attrs.get('id') and link.attrs.get('href'):
                guide_anchors.add(anchor.attrs['id'])
    for link in blocks[0].find('a'):
        target = urlsplit(link.attrs.get('href', ''))
        if not target.scheme and not target.netloc and target.path in ('', 'index.html') \
                and target.fragment in guide_anchors:
            issues.append(f'link targets a legacy guide navigation anchor: {target.fragment}')
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
    logo_images = [image for node in intro_nodes for image in node.find('img') if image.attrs.get('src') == logo]
    if logo and not logo_images:
        issues.append(f'logo is missing from the intro section: {logo}')
    for image in logo_images:
        issue = logo_size_issue(dict(image.attrs))
        if issue:
            issues.append(issue)
    return issues


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pages', nargs='+', type=Path)
    parser.add_argument('--logo', help='expected logo filename in the intro section')
    parser.add_argument('--sources', action='store_true',
                        help='check the logo image sizes of documentation sources instead of rendered pages')
    args = parser.parse_args()
    failures = 0
    if args.sources:
        for path in args.pages:
            try:
                issues = inspect_logo_sources(path.read_text(encoding='utf-8', errors='replace'))
            except OSError as error:
                issues = [(0, str(error))]
            for number, issue in issues:
                print(f'{path}:{number}: {issue}', file=sys.stderr)
                failures += 1
        print(f'Checked {len(args.pages)} sources; {failures} logo size errors')
        return int(bool(failures))
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
