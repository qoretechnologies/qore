#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Print the binary module API (<major>.<minor>) declared by include/qore/ModuleManager.h.

debian/rules gives libqore20 "Provides: qore-module-abi-<major>.<minor>" from it, so that a binary module package can
depend on the module API that it was built for (design/module-api-version.md).
"""
import re
import sys


def module_api(text):
    """Return "<major>.<minor>" from the text of ModuleManager.h; raise ValueError if it is not declared exactly once."""
    parts = []
    for name in ("MAJOR", "MINOR"):
        found = re.findall(rf"^#define QORE_MODULE_API_{name}[ \t]+([0-9]+)[ \t]*$", text, re.MULTILINE)
        if len(found) != 1:
            raise ValueError(f"QORE_MODULE_API_{name} must be defined exactly once as a number, found {found!r}")
        parts.append(str(int(found[0])))
    return ".".join(parts)


def main(argv):
    if len(argv) != 2:
        print(f"usage: {argv[0]} <ModuleManager.h>", file=sys.stderr)
        return 2
    with open(argv[1], encoding="utf-8") as header:
        print(module_api(header.read()))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
