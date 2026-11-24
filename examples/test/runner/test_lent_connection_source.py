#!/usr/bin/env python3
"""Every client of a connection class in qlib is created through AbstractConnection::getLentTargetSource().

Restricted code that was lent a connection can change the connection's members, from which its client is created, also
from another thread while the client is created.  For such code getLentTargetSource() checks the members and returns
a private copy of the connection with the members that its creator set, so the client must be created from the object
that it returns (qore#7327).  The check finds the calls of getImpl() and getAsyncImpl() on the connection itself
(``getImpl(...)``, not ``src.getImpl(...)``) in the methods of connection classes, other than in the methods that run
on the object that getLentTargetSource() returned: getImpl() and getAsyncImpl() themselves, and the implementation
methods that the base class calls on it (getDataProviderImpl() and the file location methods).  A connection class
whose clients are bound to its creator's target by a rule of their own (its creator proof: the HTTP-based connection
classes) is not checked.  ``self.getImpl(...)`` counts as a call on the connection itself, and every getDataProvider()
of a checked class must create the data provider with getLentDataProviderSource(), which also checks its constructor
options.  Clients created directly from the connection's members (without getImpl()) are not detected; the qtest
lent-nonhttp-connection-target.qtest covers the classes that do so.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import functools
from pathlib import Path
import re
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
QLIB = ROOT / "qlib"

# the base class of connections
BASE = "AbstractConnection"
# classes whose clients are bound to the creator's target by its creator proof (hasCreatorBoundClients() is True)
CREATOR_BOUND = {"HttpBasedConnection", "HttpConnection"}
# methods that run on the object that getLentTargetSource() returned
CONFINED_METHODS = {
    "getImpl",
    "getAsyncImpl",
    "getDataProviderImpl",
    "getFileLocationRedirectImpl",
    "getFileLocationHandlerImpl",
}

CLASS_RE = re.compile(r"\bclass\s+([A-Za-z_]\w*)(?:\s+inherits\s+([^{]*))?$", re.S)
METHOD_RE = re.compile(r"([A-Za-z_]\w*)\s*\([^{]*$", re.S)
CALL_RE = re.compile(r"(getImpl|getAsyncImpl)\s*\(")


def strip_code(src):
    """Returns the source with comments and the contents of string literals removed; lines are kept"""
    out = []
    i = 0
    n = len(src)
    while i < n:
        c = src[i]
        if src.startswith("/*", i):
            end = src.find("*/", i + 2)
            end = n if end < 0 else end + 2
            out.append("\n" * src.count("\n", i, end))
            i = end
        elif c == "#":
            end = src.find("\n", i)
            i = n if end < 0 else end
        elif c == '"':
            j = i + 1
            while j < n and src[j] != '"':
                if src[j] == "\\":
                    j += 1
                j += 1
            out.append('""' + "\n" * src.count("\n", i, min(j + 1, n)))
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


@functools.lru_cache(maxsize=None)
def scan(path):
    """Returns the classes of a file, the calls of getImpl()/getAsyncImpl() on the object itself (also through
    ``self.``) and the methods with their bodies

    @return ({class: [parent, ...]}, [(class, method, line, call), ...], [(class, method, line, body), ...])
    """
    src = strip_code(path.read_text(errors="replace"))
    classes = {}
    calls = []
    methods = []
    # the open blocks: (kind, class, method, start, line); kind is "class", "method" or "block"
    stack = []
    header_start = 0
    line = 1
    i = 0
    n = len(src)
    while i < n:
        c = src[i]
        if c == "\n":
            line += 1
        elif c in ";}":
            if c == "}" and stack:
                top = stack.pop()
                if top[0] == "method":
                    methods.append((top[1], top[2], top[4], src[top[3]:i]))
            header_start = i + 1
        elif c == "{":
            header = src[header_start:i].strip()
            header_start = i + 1
            mc = CLASS_RE.search(header)
            if mc:
                classes[mc.group(1)] = [p.split()[-1].split("::")[-1] for p in (mc.group(2) or "").split(",")
                    if p.strip()]
                stack.append(("class", mc.group(1), None, i, line))
            elif stack and stack[-1][0] == "class":
                mm = METHOD_RE.search(header)
                stack.append(("method", stack[-1][1], mm.group(1) if mm else None, i, line))
            elif stack:
                stack.append(("block",) + stack[-1][1:])
            else:
                stack.append(("block", None, None, i, line))
        elif stack and stack[-1][0] != "class" and stack[-1][1]:
            m = CALL_RE.match(src, i) if (c == "g" and (i == 0 or src.endswith("self.", 0, i)
                or not (src[i - 1].isalnum() or src[i - 1] in "._:"))) else None
            if m:
                calls.append((stack[-1][1], stack[-1][2], line, m.group(1)))
                i = m.end()
                continue
        i += 1
    return classes, calls, methods


def qlib_sources():
    return sorted(list(QLIB.glob("*.q[mc]")) + list(QLIB.glob("*/*.q[mc]")))


def connection_classes(files):
    """Returns the names of the connection classes that are checked, and of all connection classes"""
    parents = {}
    for path in files:
        classes = scan(path)[0]
        for name, ps in classes.items():
            parents.setdefault(name, set()).update(ps)

    def derives(name, targets, seen=()):
        if name in targets:
            return True
        if name in seen:
            return False
        return any(derives(p, targets, seen + (name,)) for p in parents.get(name, ()))

    conns = {n for n in parents if derives(n, {BASE})}
    bound = {n for n in conns if derives(n, CREATOR_BOUND)}
    return conns - bound, conns


class LentConnectionSourceTest(unittest.TestCase):
    maxDiff = None

    def test_no_bare_client_creation(self):
        files = qlib_sources()
        checked, all_conns = connection_classes(files)
        # the check sees the connection classes that qore#7327 confines
        for name in ("AbstractConnection", "TelnetConnection", "SmtpConnection", "Pop3Connection", "ImapConnection",
                     "FtpConnection", "RedisConnection", "MemcachedConnection", "MllpConnection", "MongoDbConnection",
                     "ConnectConnection", "WebSocketConnectionObject", "ServerSentEventConnection",
                     "DatasourceConnection"):
            self.assertIn(name, checked, name)
        # and does not check the HTTP-based ones
        self.assertIn("RestConnection", all_conns)
        self.assertNotIn("RestConnection", checked)
        found = []
        for path in files:
            for cls, method, line, call in scan(path)[1]:
                if cls in checked and method not in CONFINED_METHODS:
                    found.append(f"{path.relative_to(ROOT)}:{line}: {cls}::{method}() calls {call}() on itself")
        self.assertEqual([], found, "create the client from the object that getLentTargetSource() returns")

    def test_data_providers_confined(self):
        files = qlib_sources()
        checked = connection_classes(files)[0]
        found = []
        for path in files:
            for cls, method, line, body in scan(path)[2]:
                if cls in checked and method == "getDataProvider" and "getLentDataProviderSource(" not in body \
                        and not re.search(r"\bthrow\b", body):
                    found.append(f"{path.relative_to(ROOT)}:{line}: {cls}::getDataProvider()")
        self.assertEqual([], found, "a data provider must be created with getLentDataProviderSource()")

    def test_detects_bare_call(self):
        # a fail-first check of the scanner itself on a source with a bare call, a confined call and comments
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "X.qc"
            p.write_text("""
public class XConnection inherits AbstractConnection {
    #! a doc comment that names getImpl()
    /** getImpl() in a block comment */
    object leak(int a,
            int b) {
        string s = "getImpl(";
        if (a) {
            return getImpl(False);
        }
        if (b) {
            return self.getImpl(False);
        }
        return cast<XConnection>(getLentTargetSource()).getImpl(False);
    }

    AbstractDataProvider getDataProvider(*hash<auto> constructor_options) {
        return new XDataProvider({"x": get()} + constructor_options);
    }

    private object getImpl(bool connect = True, *hash<auto> rtopts) {
        return getImpl(connect, rtopts);
    }
}
""")
            self.assertEqual([("XConnection", "leak", 9, "getImpl"), ("XConnection", "leak", 12, "getImpl"),
                              ("XConnection", "getImpl", 22, "getImpl")], scan(p)[1])
            self.assertEqual([("XConnection", "leak", 6), ("XConnection", "getDataProvider", 17),
                              ("XConnection", "getImpl", 21)], [m[:3] for m in scan(p)[2]])


if __name__ == "__main__":
    unittest.main()
