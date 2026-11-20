#!/usr/bin/env python3
"""The static data members of the public classes of the standard modules that a subclass can read are reviewed.

A static member of a class is one storage for the whole process: every caller of a standard module, and so every
tenant of a host that runs tenants in sandboxed Programs, shares it.  A static member declared in a "private" block is
visible to every subclass (private:hierarchy), and sandboxed code can derive a class from a public class of a module
and read it (qore#6748); since qore#5664 it cannot write it, but it can read it and call the methods of an object held
in it.  The shared state of the callers of a module (cached objects with their connections, the host's callbacks and
loggers, the state of other callers' operations) is therefore declared private:internal, which no subclass can see.

These tests scan the sources of the standard modules (qlib) for static data members that a subclass of a public class
that is not final can see, and fail for each one that module-shared-statics.txt next to this file does not list, and
for each entry of the list whose member no longer exists.  A new member is therefore either private:internal, or
added to the list with the category that says why every caller may read it:

    lock         a lock of the class's own state; its only effect is interference, which qore#6750 covers
    class        the class's own Reflection::Class object
    child-map    the map of the classes of the child providers of a data provider
    module-data  data of the module itself, the same for every caller: schemas, API definitions and data types that
                 the module loads from its own sources or from a public source, and the objects built from them
    scalar       a flag, a counter or a scalar cache that holds no data of a caller
    callbacks    a registry of callbacks that qore#6743 covers
    factories    a registry of factories that qore#6744 covers

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]
QLIB = ROOT / "qlib"
LIST = Path(__file__).resolve().parent / "module-shared-statics.txt"

CATEGORIES = {"lock", "class", "child-map", "module-data", "scalar", "callbacks", "factories"}

CLASS = re.compile(r"^\s*((?:public|final|abstract|\s)*)class\s+([A-Za-z_]\w*)\b")
BLOCK = re.compile(r"^\s*(private:internal|private:hierarchy|private|public)\s*\{")
STATIC = re.compile(r"^\s*(?:transient\s+)?static\s+\S")
STRING = re.compile(r'"(\\.|[^"\\])*"')
TEMPLATE = re.compile(r"<[^<>]*>")


def member_name(decl):
    """Returns the name of the member that a static declaration declares."""
    d = decl
    while TEMPLATE.search(d):
        d = TEMPLATE.sub("", d)
    m = re.match(r"\s*(?:transient\s+)?static\s+(?:\*?[\w:]+\s+)?([A-Za-z_]\w*)\s*(?:=|;|\(|$)", d)
    return m.group(1) if m else None


def scan():
    """Returns (path, class, member) for each static data member in a private (hierarchy) block of a public class
    that is not final."""
    found = []
    for path in sorted(list(QLIB.rglob("*.qm")) + list(QLIB.rglob("*.qc"))):
        rel = path.relative_to(ROOT).as_posix()
        depth = 0
        classes = []
        block = None
        in_comment = False
        for line in path.read_text(encoding="utf-8", errors="replace").split("\n"):
            s = line
            if in_comment:
                if "*/" not in s:
                    continue
                s = s[s.index("*/") + 2:]
                in_comment = False
            s = STRING.sub('""', s)
            s = re.sub(r"/\*.*?\*/", "", s)
            if "/*" in s:
                s = s[:s.index("/*")]
                in_comment = True
            s = re.sub(r"#.*$", "", s)
            m = CLASS.match(s)
            if m and "{" in s:
                classes.append((m.group(2), depth, "public" in m.group(1), "final" in m.group(1)))
            if classes:
                bm = BLOCK.match(s)
                if bm:
                    block = (bm.group(1), depth + 1)
                name, _, public, final = classes[-1]
                if (block and depth == block[1] and block[0] in ("private", "private:hierarchy") and public
                        and not final and STATIC.match(s)):
                    found.append((rel, name, member_name(s)))
            depth += s.count("{") - s.count("}")
            while classes and depth <= classes[-1][1]:
                classes.pop()
            if block and depth < block[1]:
                block = None
    return found


def read_list():
    """Returns {(path, class, member): category} from the list."""
    rv = {}
    for n, line in enumerate(LIST.read_text(encoding="utf-8").split("\n"), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        category, path, member = line.split()
        cls, name = member.split("::")
        rv[(path, cls, name)] = (category, n)
    return rv


class ModuleSharedStaticsTest(unittest.TestCase):
    def test_scan_finds_members(self):
        found = scan()
        self.assertGreater(len(found), 100, "the scan finds the static members of the standard modules")
        self.assertFalse([f for f in found if not f[2]], "a member whose name the scan cannot read")
        # a member of a private:internal block is not found
        self.assertNotIn(("qlib/SqlUtil/SqlUtil.qm", "TableCache", "cache"), found)

    def test_members_are_reviewed(self):
        listed = read_list()
        found = scan()
        missing = ["%s %s::%s" % f for f in found if f not in listed]
        self.assertEqual([], missing, "static members that a subclass of a public class can read, which are neither "
                         "private:internal nor listed in %s with a category (see this test's description)" % LIST.name)

    def test_list_is_current(self):
        listed = read_list()
        found = set(scan())
        stale = ["line %d: %s %s::%s" % (v[1], k[0], k[1], k[2]) for k, v in listed.items() if k not in found]
        self.assertEqual([], stale, "entries of %s whose member is no longer one that a subclass can read" % LIST.name)
        bad = ["line %d: %s" % (v[1], v[0]) for v in listed.values() if v[0] not in CATEGORIES]
        self.assertEqual([], bad, "unknown categories")


if __name__ == "__main__":
    unittest.main()
