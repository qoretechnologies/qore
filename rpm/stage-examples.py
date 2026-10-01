#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Install user examples without the development test tree or checkout metadata."""
from pathlib import Path
import shutil
import sys


def stage(source, destination):
    source, destination = Path(source), Path(destination)
    if not source.is_dir() or source.is_symlink():
        raise ValueError('Examples must be a real directory')
    if destination.exists() or destination.is_symlink():
        raise ValueError('Examples destination must be new')
    files = []
    for path in sorted(source.rglob('*')):
        relative = path.relative_to(source)
        if relative.parts[0] == 'test' or any(p in ('.git', '.gitignore', '.gitattributes') for p in relative.parts):
            continue
        if path.is_symlink():
            raise ValueError('Example symlinks are not supported: ' + str(relative))
        if path.is_file():
            data = path.read_bytes()
            first, separator, body = data.partition(b'\n')
            if first.startswith(b'#!/usr/bin/env '):
                interpreter = first.removeprefix(b'#!/usr/bin/env ')
                if interpreter != b'qore':
                    raise ValueError('Unknown example interpreter: ' + str(relative))
                data = b'#!/usr/bin/qore' + separator + body
            files.append((path, relative, data))
    # Validate every input before exposing any output.
    destination.mkdir(parents=True)
    for path, relative, data in files:
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        shutil.copystat(path, target)
        target.chmod(0o755 if data.startswith(b'#!') else 0o644)


if __name__ == '__main__':
    stage(*sys.argv[1:])
