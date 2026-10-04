#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: GPL-2.0-only
"""Exercise real HTML search generation for mixed ASCII and Unicode headings."""

import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--doxygen", default="doxygen")
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix="doxygen-unicode-") as temporary:
    root = Path(temporary)
    (root / "headings.dox").write_text("""/**
@page examples Examples
@section acceptor GSSAPI Acceptor Context — Service Endpoint
Service endpoint documentation.
@section initiator GSSAPI Initiator Context — Step Loop
Initiator documentation.
@section functions Context Functions
Context function documentation.
*/
""", encoding="utf-8")
    (root / "Doxyfile").write_text("""PROJECT_NAME = UnicodeSearch
INPUT = headings.dox
OUTPUT_DIRECTORY = output
GENERATE_HTML = YES
GENERATE_LATEX = NO
SEARCHENGINE = YES
QUIET = YES
HAVE_DOT = NO
HTML_TIMESTAMP = NO
""")
    subprocess.run([args.doxygen, "Doxyfile"], cwd=root, check=True, timeout=60,
                   env={**os.environ, "LC_ALL": "C", "SOURCE_DATE_EPOCH": "1700000000"})
    rows = []
    for path in sorted((root / "output/html/search").glob("all_*.js")):
        rows.extend(line.strip() for line in path.read_text(encoding="utf-8").splitlines()
                    if re.match(r"\s*\['context_20", line))
    assert len(rows) == 3, rows
    expected = ["Context Functions", "GSSAPI Acceptor Context — Service Endpoint",
                "GSSAPI Initiator Context — Step Loop"]
    for row, title in zip(rows, expected):
        assert title in row, (title, row)
    assert all(f"examples.html#{anchor}" in "\n".join(rows)
               for anchor in ("functions", "acceptor", "initiator")), rows
    print("PASS: ASCII/Unicode search ordering and all section links")
