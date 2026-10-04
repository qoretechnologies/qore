#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: GPL-2.0-only
"""Check that generated or installed man pages contain version and date values."""

import argparse
from datetime import datetime
import gzip
from pathlib import Path
import re
import subprocess


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--man-dir", type=Path, default=Path("/usr/share/man/man1"))
parser.add_argument("--doxygen", default="doxygen")
args = parser.parse_args()
version = subprocess.check_output([args.doxygen, "--version"], text=True, timeout=30).strip()
if not re.fullmatch(r"\d+\.\d+\.\d+(?:[\w.+-]*)", version):
    raise RuntimeError(f"unexpected Doxygen version: {version!r}")
for name in ("doxygen", "doxyindexer", "doxysearch", "doxywizard"):
    path = args.man_dir / (name + ".1")
    if path.is_file():
        text = path.read_text(encoding="utf-8")
    else:
        with gzip.open(str(path) + ".gz", "rt", encoding="utf-8") as page:
            text = page.read()
    if re.search(r"@[A-Z_]+@", text):
        raise RuntimeError(f"unconfigured template in {path}")
    header = re.search(r'^\.TH "?([A-Z]+)"? "1" "(\d{2}-\d{2}-\d{4})" "([^"]+)"', text, re.MULTILINE)
    tool = "doxysearch.cgi" if name == "doxysearch" else name
    if not header or header[1] != name.upper() or header[3] != f"{tool} {version}":
        raise RuntimeError(f"missing date or incorrect version in {path}")
    datetime.strptime(header[2], "%d-%m-%Y")
print("PASS: all four man pages have configured dates and matching tool versions")
