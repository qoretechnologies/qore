#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Install Qore sources that LLVM's synthetic <aot> DWARF cannot enumerate."""
import argparse
import os
from pathlib import Path
import shutil
import tempfile


def install_sources(buildroot, debug_prefix, sources, source_root=None):
    root = Path(source_root or Path.cwd()).resolve()
    buildroot = Path(buildroot).resolve(strict=True)
    prefix = Path(debug_prefix)
    if (not prefix.is_absolute() or ".." in prefix.parts
            or not prefix.is_relative_to("/usr/src/debug") or prefix == Path("/usr/src/debug")):
        raise ValueError("Use a directory below /usr/src/debug")
    destination = buildroot / prefix.relative_to("/")
    if not destination.resolve().is_relative_to(buildroot):
        raise ValueError("Debug-source destination escapes the buildroot")
    selected = {}
    for name in sources:
        source = root / name
        if not source.exists():
            raise ValueError("Missing Qore source path: " + str(name))
        if ".." in source.parts or not source.resolve().is_relative_to(root):
            raise ValueError("Qore debug sources must be files inside the source root")
        candidates = source.rglob("*") if source.is_dir() else [source]
        for path in candidates:
            if path.suffix not in (".qm", ".qc", ".q"):
                continue
            if not path.resolve(strict=True).is_relative_to(root) or not path.is_file():
                raise ValueError("Qore debug sources must be files inside the source root")
            relative = path.relative_to(root)
            target = destination / relative
            if not target.resolve().is_relative_to(destination.resolve()):
                raise ValueError("Qore debug-source target escapes its destination")
            selected[relative] = path
    # Validate all inputs before creating output. Each file is then replaced
    # atomically, preserving the source timestamp for reproducible RPMs.
    for relative, path in sorted(selected.items()):
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        descriptor, temporary = tempfile.mkstemp(prefix=".qore-source-", dir=target.parent)
        try:
            with os.fdopen(descriptor, "wb") as output, path.open("rb") as source:
                shutil.copyfileobj(source, output)
            shutil.copystat(path, temporary)
            os.chmod(temporary, 0o644)
            os.replace(temporary, target)
        finally:
            Path(temporary).unlink(missing_ok=True)
    return len(selected)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("buildroot")
    parser.add_argument("debug_prefix")
    parser.add_argument("sources", nargs="+")
    args = parser.parse_args()
    install_sources(args.buildroot, args.debug_prefix, args.sources)


if __name__ == "__main__":
    main()
