#!/usr/bin/env python3
"""The process-wide registries of the standard modules refuse changes from sandboxed code.

A registry, lookup, resolver, serializer, codec, schema or type cache that a standard module keeps in a static
member or a global variable is used by every caller of the module, and so by every tenant of a host that runs tenants
in sandboxed Programs: an entry that one tenant registers, replaces or removes changes what the module does for the
others (qore#6747).  A functional domain tag (such as PROCESS_CONTROL) does not refuse sandboxed code that the host
gives the domain, so every method that changes such a registry refuses sandboxed callers itself, with
DataProvider::checkSharedRegistration() (DATA-PROVIDER-REGISTRATION-DENIED), the data provider catalog's checks, or
SandboxManager::isSandboxed().

These tests scan the sources of the standard modules (qlib) for the public static methods of public classes and the
public functions whose names are those of a registry's mutators (register..., deregister..., unregister...,
set...Lookup, set...Resolver, set...Filter, set...Logger, ..., replace..., remove..., clear...), and for the
mutators of the classes whose process-wide instance a module gives to its callers (SharedInstanceClasses), and fail
for each one whose code calls none of those checks and that shared-registry-checks.txt next to this file does not
list, and for each entry of the list that no longer exists.  The list gives each entry a category:

    delegates     every change is made through a method that makes the check (ex: a wrapper that registers the
                  module's codecs with RestSchemaActionCodecs::registerCodec())
    module-load   the registration is made only while a module is being loaded, and returns its entry only to it
    callbacks     a registry of callbacks that qore#6743 covers
    factories     a registry of factories that qore#6744 covers
    tokens        a cache of credential tokens that qore#6745 covers

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]
QLIB = ROOT / "qlib"
LIST = Path(__file__).resolve().parent / "shared-registry-checks.txt"

CATEGORIES = {"delegates", "module-load", "callbacks", "factories", "tokens"}

#! the classes whose process-wide instance a module gives to its callers: all of their mutators are checked
SharedInstanceClasses = {"DataProviderTypeCache", "YamlTagHandlerRegistry"}

NAME = re.compile(r"^(register|deregister|unregister|tryRegister|set[A-Z]\w*(Lookup|Resolver|Filter|Callback|Callbacks|"
                  r"Handler|Serializer|Deserializer|Hook|Logger|Codec|Factory|BuildMode|EnvironmentVariable)$|replace|"
                  r"registerOrReplace|remove(Type|Scheme|Handler|Codec|Factory|App|Cached|Schema|Resolver|Lookup|"
                  r"Catalog|Listener)|clear|add\w*ToCache$|\w*_register_|\w*_unregister_)")
CHECK = re.compile(r"isSandboxed\(\)|checkRegistration\(|checkSharedRegistration\(|checkChildRegistration\(|"
                   r"mayRegisterShared\(|checkChange\(|checkNotGoverned\(|checkNotSandboxed\(|"
                   r"checkProcessWideRegistration\(|isProcessWideRegistrationAllowed\(")

#! the child data provider registries: a child class that one caller registers is created for every caller that
#! browses the provider, with that caller's connection, so these mutators must refuse sandboxed callers themselves
#! and cannot be listed (qore#6744); the base class's method only raises UNSUPPORTED-ERROR
CHILD_REGISTRY = re.compile(r"::(register|deregister)Child$")
CHILD_REGISTRY_BASE = {("qlib/DataProvider/AbstractDataProvider.qc", "AbstractDataProvider::registerChild")}
CLASS = re.compile(r"^\s*((?:public|final|abstract|\s)*)class\s+([A-Za-z_]\w*)\b")
STATIC = re.compile(r"^\s*((?:public|private(?::\w+)?|static|synchronized|abstract|\s)*)\bstatic\b\s*((?:public|"
                    r"private(?::\w+)?|synchronized|\s)*)(?:[\w:<>*,\s]+?\s+)?([A-Za-z_]\w*)\s*\([^;]*$")
METHOD = re.compile(r"^\s*((?:public|private(?::\w+)?|synchronized|\s)*)(?:[\w:<>*,\s]+?\s+)?([A-Za-z_]\w*)\s*\([^;]*$")
SUB = re.compile(r"^\s*public\s+(?:[\w:<>*,\s]+?\s+)?sub\s+([A-Za-z_]\w*)\s*\(")


def body(lines, i):
    """Returns the text of the function or method that starts at line i."""
    depth = 0
    started = False
    out = []
    for j in range(i, min(i + 600, len(lines))):
        out.append(lines[j])
        depth += lines[j].count("{") - lines[j].count("}")
        if "{" in lines[j]:
            started = True
        if started and depth <= 0:
            break
    return "\n".join(out)


def scan():
    """Returns {(path, name): checked} for the mutators of process-wide registries."""
    found = {}
    for path in sorted(list(QLIB.rglob("*.qm")) + list(QLIB.rglob("*.qc"))):
        rel = path.relative_to(ROOT).as_posix()
        lines = path.read_text(encoding="utf-8", errors="replace").split("\n")
        depth = 0
        classes = []
        for i, line in enumerate(lines):
            s = re.sub(r'"(\\.|[^"\\])*"', '""', line)
            s = re.sub(r"#.*$", "", s)
            m = CLASS.match(s)
            if m and "{" in s:
                classes.append((m.group(2), depth, "public" in m.group(1)))
            cls = classes[-1] if classes and depth == classes[-1][1] + 1 else None
            name = None
            if cls and cls[2]:
                sm = STATIC.match(s)
                if sm and "private" not in sm.group(1) + sm.group(2) and NAME.match(sm.group(3)):
                    name = "%s::%s" % (cls[0], sm.group(3))
                elif not sm and cls[0] in SharedInstanceClasses:
                    mm = METHOD.match(s)
                    if (mm and "private" not in mm.group(1) and NAME.match(mm.group(2))
                            and mm.group(2) not in ("constructor", "destructor")):
                        name = "%s::%s" % (cls[0], mm.group(2))
            elif not classes:
                fm = SUB.match(s)
                if fm and NAME.match(fm.group(1)):
                    name = fm.group(1)
            if name:
                found[(rel, name)] = bool(CHECK.search(body(lines, i)))
            depth += s.count("{") - s.count("}")
            while classes and depth <= classes[-1][1]:
                classes.pop()
    return found


def read_list():
    rv = {}
    for n, line in enumerate(LIST.read_text(encoding="utf-8").split("\n"), 1):
        line = line.split("#", 1)[0].strip()
        if line:
            category, path, name = line.split()
            rv[(path, name)] = (category, n)
    return rv


class SharedRegistryChecksTest(unittest.TestCase):
    def test_scan_finds_mutators(self):
        found = scan()
        self.assertGreater(len(found), 30, "the scan finds the registry mutators of the standard modules")
        self.assertTrue(found.get(("qlib/RestSchemaActions/RestSchemaActionCodecs.qc",
                                   "RestSchemaActionCodecs::registerCodec")), "a checked mutator is found as checked")

    def test_mutators_are_checked(self):
        listed = read_list()
        unchecked = ["%s %s" % k for k, checked in sorted(scan().items()) if not checked and k not in listed]
        self.assertEqual([], unchecked, "mutators of process-wide registries that refuse no sandboxed caller and that "
                         "%s does not list with a category (see this test's description)" % LIST.name)

    def test_child_registries_are_checked(self):
        found = scan()
        sites = {k: v for k, v in found.items() if CHILD_REGISTRY.search(k[1]) and k not in CHILD_REGISTRY_BASE}
        self.assertGreater(len(sites), 30, "the scan finds the child data provider registries")
        unchecked = ["%s %s" % k for k, checked in sorted(sites.items()) if not checked]
        self.assertEqual([], unchecked, "child data provider registries that do not refuse sandboxed callers")
        listed = ["%s %s" % k for k in sorted(read_list()) if CHILD_REGISTRY.search(k[1])]
        self.assertEqual([], listed, "child data provider registries cannot be listed in %s" % LIST.name)

    def test_list_is_current(self):
        listed = read_list()
        found = scan()
        stale = ["line %d: %s %s" % (v[1], k[0], k[1]) for k, v in listed.items() if found.get(k) is not False]
        self.assertEqual([], stale, "entries of %s that are not unchecked mutators" % LIST.name)
        bad = ["line %d: %s" % (v[1], v[0]) for v in listed.values() if v[0] not in CATEGORIES]
        self.assertEqual([], bad, "unknown categories")


if __name__ == "__main__":
    unittest.main()
