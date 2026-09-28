#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Remove Doxygen graph build caches from staged documentation, keeping rendered graphs."""

from pathlib import Path
import re
import sys


def clean(root):
    removed = 0
    for path in root.rglob("*"):
        if path.suffix not in (".map", ".md5") or not path.is_file():
            continue
        # These are graph-generation caches beside a rendered graph. Do not
        # remove unrelated resources such as JavaScript source maps/checksums.
        if not any(path.with_suffix(ext).is_file() for ext in (".svg", ".png")):
            continue
        data = path.read_bytes().strip()
        if ((path.suffix == ".md5" and re.fullmatch(rb"[0-9a-fA-F]{32}", data))
                or (path.suffix == ".map" and data.startswith(b"<map ") and data.endswith(b"</map>"))):
            path.unlink()
            removed += 1
    return removed


if __name__ == "__main__":
    if len(sys.argv) != 2 or not Path(sys.argv[1]).is_dir():
        sys.exit("Usage: clean-doxygen-cache.py STAGED_DOCUMENTATION_DIRECTORY")
    print(f"Removed {clean(Path(sys.argv[1]))} Doxygen graph cache files")
