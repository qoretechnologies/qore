#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Run an ELF editing command without losing Qore's appended AOT metadata.

Usage: preserve-aot-metadata.py MODULE_ROOT COMMAND [ARG ...]
The footer layout is defined in include/qore/intern/QoreAOTBinary.h. RPM's
strip/debuglink operations discard bytes outside ELF sections, including the
optional-module guard which controls source fallback after installing modules.
"""

from pathlib import Path
import os
import struct
import subprocess
import sys


FOOTER = struct.Struct("<Q4sI")
MAGICS = {b"QAMD", b"QPCM", b"QAOM"}


def read_trailers(path: Path) -> bytes:
    """Read the contiguous, versioned Qore trailer chain, or empty bytes."""
    with path.open("rb") as stream:
        end = stream.seek(0, 2)
        start = end
        seen = set()
        while start >= FOOTER.size:
            stream.seek(start - FOOTER.size)
            size, magic, version = FOOTER.unpack(stream.read(FOOTER.size))
            if magic not in MAGICS:
                break
            if version != 1 or magic in seen or not size or size > start - FOOTER.size:
                raise ValueError(f"{path}: invalid {magic!r} AOT trailer")
            seen.add(magic)
            start -= size + FOOTER.size
        stream.seek(start)
        return stream.read(end - start)


def preserve(root: Path, command: list[str]) -> None:
    """Restore metadata after editing, including when the command fails."""
    saved = {}
    for path in sorted(root.rglob("*.qmod")):
        if path.is_symlink() or not path.is_file():
            continue
        trailers = read_trailers(path)
        if trailers:
            saved[path] = trailers
    errors = []
    try:
        subprocess.run(command, check=True)
    except BaseException as error:
        errors.append(error)
    # A failed restore must not prevent restoration of the remaining modules.
    for path, trailers in saved.items():
        try:
            if path.is_symlink():
                raise ValueError(f"{path}: ELF editing replaced the module with a symlink")
            stat = path.stat()
            current = read_trailers(path)
            if current == trailers:
                continue  # Commands that retain metadata must not duplicate it.
            if current:
                raise ValueError(f"{path}: ELF editing changed the AOT metadata")
            with path.open("ab") as stream:
                stream.write(trailers)
            if read_trailers(path) != trailers:
                raise ValueError(f"{path}: failed to restore AOT metadata")
            os.utime(path, ns=(stat.st_atime_ns, stat.st_mtime_ns), follow_symlinks=False)
        except Exception as error:
            errors.append(error)
    if len(errors) == 1:
        raise errors[0]
    if errors:
        raise BaseExceptionGroup("ELF processing and AOT metadata restoration failed", errors)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    preserve(Path(sys.argv[1]), sys.argv[2:])
