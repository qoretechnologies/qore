#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
# documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
# persons to whom the Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
# OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
"""The memory of the compiles of a CI build (qore#7780)

A CI build job is killed when its container exceeds its memory limit (the libqore single compilation unit alone needs
~15 GiB in a debug build).  This records what the build used, so that check-build-warnings.py can warn before a
growth reaches the limit:

- as the compiler launcher of the build (CMAKE_<LANG>_COMPILER_LAUNCHER): runs the compiler command that follows and
  appends its peak resident memory, its time and its output file to the log named by QORE_CI_COMPILE_MEMORY_LOG
  (nothing is logged without the variable); the exit status is the compiler's
- report LOG OUTPUT: writes the report of the build: the peak memory and the limit of the build's cgroup (the
  container, which the limit applies to) when the system has them, and the compiles that used the most memory

The report has one "key=value" per line: peak_bytes, limit_bytes (absent when unknown or unlimited), compiles, then
"compile=<bytes>\\t<seconds>\\t<output>" for the largest compiles, largest first.
"""

import os
from pathlib import Path
import resource
import subprocess
import sys
import time

# the compiles listed in the report
TOP_COMPILES = 10


def child_peak_bytes():
    """Returns the peak resident memory of the largest child process that has ended, in bytes"""
    rss = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
    # kilobytes on Linux, bytes on macOS
    return rss if sys.platform == "darwin" else rss * 1024


def output_of(args):
    """Returns the output file of a compiler command (the argument after -o), or the last argument"""
    for i, a in enumerate(args):
        if a == "-o" and i + 1 < len(args):
            return args[i + 1]
    return args[-1] if args else ""


def launch(args):
    log = os.environ.get("QORE_CI_COMPILE_MEMORY_LOG")
    start = time.monotonic()
    try:
        rc = subprocess.call(args)
    except OSError as e:
        print(f"ci-compile-memory: cannot run {args[0]!r}: {e}", file=sys.stderr)
        return 127
    if log:
        line = f"{child_peak_bytes()}\t{time.monotonic() - start:.2f}\t{output_of(args)}\n"
        # one write of a short line in append mode: parallel compiles do not interleave their lines
        with open(log, "a") as f:
            f.write(line)
    return rc


def cgroup_values():
    """Returns (peak bytes, limit bytes) of this process's cgroup, each None when unknown or unlimited"""
    def read(path):
        try:
            v = Path(path).read_text().strip()
        except OSError:
            return None
        return int(v) if v.isdigit() else None
    rel = ""
    try:
        for line in Path("/proc/self/cgroup").read_text().splitlines():
            parts = line.split(":", 2)
            if len(parts) == 3 and parts[0] == "0":
                rel = parts[2]
    except OSError:
        pass
    # cgroup v2: the group of the process, else the root of the container's namespace
    for base in (Path("/sys/fs/cgroup") / rel.lstrip("/"), Path("/sys/fs/cgroup")):
        peak = read(base / "memory.peak")
        if peak is not None:
            return peak, read(base / "memory.max")
    # cgroup v1
    peak = read("/sys/fs/cgroup/memory/memory.max_usage_in_bytes")
    limit = read("/sys/fs/cgroup/memory/memory.limit_in_bytes")
    # an unlimited v1 group has a huge number as its limit
    if limit is not None and limit >= 1 << 60:
        limit = None
    return peak, limit


def report(log, output):
    compiles = []
    if Path(log).is_file():
        for line in Path(log).read_text().splitlines():
            parts = line.split("\t", 2)
            if len(parts) == 3 and parts[0].isdigit():
                compiles.append((int(parts[0]), parts[1], parts[2]))
    compiles.sort(key=lambda c: -c[0])
    peak, limit = cgroup_values()
    lines = []
    if peak is not None:
        lines.append(f"peak_bytes={peak}")
    if limit is not None:
        lines.append(f"limit_bytes={limit}")
    lines.append(f"compiles={len(compiles)}")
    for rss, secs, out in compiles[:TOP_COMPILES]:
        lines.append(f"compile={rss}\t{secs}\t{out}")
    Path(output).write_text("\n".join(lines) + "\n")
    summary = [line for line in lines if not line.startswith("compile=")]
    if compiles:
        summary.append(f"largest={compiles[0][0]} ({compiles[0][2]})")
    print("compile memory: " + ", ".join(summary))
    return 0


def main(argv):
    if len(argv) == 3 and argv[0] == "report":
        return report(argv[1], argv[2])
    if not argv:
        print("usage: ci-compile-memory.py COMPILER ARGS... | report LOG OUTPUT", file=sys.stderr)
        return 2
    return launch(argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
