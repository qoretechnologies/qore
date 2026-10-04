#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: GPL-2.0-only
"""Compile the upstream string comparators with both plain-char representations."""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile


root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="doxygen-compare-") as temporary:
    for mode in ("signed", "unsigned"):
        executable = Path(temporary) / mode
        subprocess.run([
            *shlex.split(os.environ.get("CXX", "c++")), "-std=c++17", f"-f{mode}-char",
            # Match Doxygen 1.15's CMake definition for its C++17 build.
            "-DJAVACC_CHAR_TYPE=unsigned char",
            "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
            "-I", str(root / "src"), str(root / "src/qcstring.cpp"),
            str(root / "debian/tests/compare-strings.cpp"), "-o", str(executable),
        ], check=True, timeout=120)
        subprocess.run([executable], check=True, timeout=30)
