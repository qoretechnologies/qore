#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Validate the installed SDK/tool inventory and normalize script interpreters."""
from pathlib import Path
import sys

TOOLS = """qdiff qdp qfmt qget qklist qls qore-asyncapi-gen
qore-data-provider-i18n qore-i18n qore-openapi3-gen qore-onnx qpatch
qschema rest saprest schema-reverse sfrest sqlutil""".split()
TEMPLATES = """Doxyfile.in qore-logo-55x200.png qore-logo-55x151-white.png
dox_qore.css DataProvider-Full.svg elastic-logo.svg Haltian-EmpathicBuilding.svg
SqlUtil-Full.svg Qore-Q.ico header_template.html footer_template.html""".split()


def finalize(root, libdir, output):
    root, output = Path(root), Path(output)
    if libdir not in ("/usr/lib", "/usr/lib64"):
        raise ValueError("Unsupported RPM library directory")
    tool_files = [f"/usr/bin/{name}" for name in TOOLS]
    tool_files += [f"/usr/share/man/man1/{name}.1" for name in TOOLS]
    devel_files = [f"/usr/share/qore/{name}" for name in TEMPLATES]
    for name in tool_files + devel_files:
        if not (root / name.lstrip("/")).is_file():
            raise ValueError(f"Missing installed file: {name}")
    for directory in [root / "usr/bin", root / libdir.lstrip("/") / "cmake/Qore"]:
        for path in directory.iterdir():
            if path.is_file() and not path.is_symlink():
                with path.open("rb") as stream:
                    first = stream.readline(256)
                if first == b"#!/usr/bin/env qore\n":
                    path.write_bytes(b"#!/usr/bin/qore\n" + path.read_bytes()[len(first):])
    (output / "rpm-tools.files").write_text("\n".join(
        name + ("*" if name.endswith(".1") else "") for name in tool_files) + "\n")
    (output / "rpm-devel-extra.files").write_text("\n".join(devel_files) + "\n")


if __name__ == "__main__":
    finalize(sys.argv[1], sys.argv[2], Path.cwd())
