#!/usr/bin/env python3
"""Keep Doxygen search results from the current module, the language, and linked modules."""

import re
import sys
from pathlib import Path


# Qore's user-module template puts the pages and diagrams in html/ and the
# search data one level deeper. Some Graphviz maps use Doxygen's
# absolute/path/Module.tag$symbol format before it rewrites the SVG links.
HREF = re.compile(rb"""\bhref\s*=\s*["']([^"'<>]+)["']""", re.I)
PAGE_MODULE = re.compile(rb"/([\w-]+)/html(?:/|[?#]|$)")
MAP_MODULE = re.compile(rb"(?:^|[/\\])([\w-]+)\.tag\$")
# Doxygen's navigation lists contain imported namespaces even when no class,
# guide, or diagram references them. index.html is the module's own main page.
LISTING = re.compile(
    r"(?:annotated|classes|concepts|dirs|doxygen_crawl|examples|files|graph_legend|"
    r"hierarchy|inherits|interfaces|modules|namespaces|pages|topics|"
    r"(?:functions|globals|namespacemembers)(?:_(?:func|vars|type|enum|eval|prop|"
    r"evnt|rela|defs))?(?:_[a-z0-9]|_0x[0-9a-f]+)?)\.html"
)
SEARCH_MODULE = re.compile(rb'\.\./\.\./\.\./([\w-]+)/html/')
JS_STRING = rb"'[^'\\]*(?:\\.[^'\\]*)*'"
# Each row holds a search key, a display name and one or more [URL,flag,scope]
# targets. Overloads with the same display name can come from different modules.
ROW = re.compile(rb"^\s*\[" + JS_STRING + rb",\[" + JS_STRING)
TARGET = re.compile(rb",\[(" + JS_STRING + rb"),\d+," + JS_STRING + rb"\]")


def prune_shard(data, linked):
    if b"../../../" not in data:
        return data
    lines = data.splitlines()
    if len(lines) < 3 or lines[:2] != [b"var searchData=", b"["] or lines[-1] != b"];":
        return data
    rows = []
    changed = False
    for line in lines[2:-1]:
        row = line[:-1] if line.endswith(b",") else line
        head = ROW.match(row)
        if not head:
            return data
        end = head.end()
        kept = []
        while row[end:end + 1] == b",":
            target = TARGET.match(row, end)
            if not target:
                return data  # An unfamiliar Doxygen layout. Keep the original shard.
            module = SEARCH_MODULE.match(row, target.start(1) + 1)
            if module and module[1] not in linked:
                changed = True
            else:
                kept.append(target[0])
            end = target.end()
        if row[end:] != b"]]":
            return data
        if kept:
            rows.append(row[:head.end()] + b"".join(kept) + b"]]")
    if not changed:
        return data
    # Keep even empty shards: the search UI looks them up by section and letter.
    return b"var searchData=\n[\n" + b",\n".join(rows) + b"\n];\n"


def prune(html):
    search = html / "search"
    if not search.is_dir():
        return
    linked = {html.parent.name.encode()}
    for page in html.iterdir():
        if page.suffix in (".html", ".svg", ".map") and not LISTING.fullmatch(page.name):
            for href in HREF.findall(page.read_bytes()):
                module = PAGE_MODULE.search(href)
                if not module and page.suffix == ".map":
                    module = MAP_MODULE.search(href)
                if module:
                    linked.add(module[1])
    for shard in search.glob("*_*.js"):
        data = shard.read_bytes()
        result = prune_shard(data, linked)
        if result != data:
            tmp = shard.with_suffix(".js.tmp")
            tmp.write_bytes(result)
            tmp.replace(shard)


if __name__ == "__main__":
    prune(Path(sys.argv[1]))
