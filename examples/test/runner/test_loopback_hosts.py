#!/usr/bin/env python3
"""A test whose servers listen on 127.0.0.1 connects its clients to 127.0.0.1, not to localhost (qore#7469)

``localhost`` resolves to ``::1`` first on a host with an IPv6 loopback address.  A client of such a test that connects
to ``localhost`` tries ``::1`` first, at the port number of a server that listens only on ``127.0.0.1``: normally
``::1`` refuses it and the client falls back to ``127.0.0.1``, but when a test running beside it (``QORE_TEST_JOBS``)
listens on ``::1`` at the same port number, the client reaches that test's server instead, and fails.

A test fails this check when it listens on ``127.0.0.1`` (a listener or socket bound to that address) and also
connects to ``localhost`` (a URL or an address whose host is ``localhost``).  A test that needs the name
``localhost``, for example to check the TLS host name, listens on both loopback addresses and declares, on a line of
its own,

    # test-loopback: localhost - <why>

A test that does not connect to ``localhost`` declares nothing.

The check reads literal addresses: a listener or client whose address is held in a variable, or built at run time,
is not seen, nor are the helper modules that tests load, so a test author keeps to the rule in those as well.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[3]
TESTS = ROOT / "examples/test"

#: a declaration that a test needs the name localhost, with the reason
MARKER = re.compile(r"^# test-loopback: localhost - (?P<why>\S(?:.*\S)?)$")
#: any line that looks like a declaration, which must then be one
ANY_MARKER = re.compile(r"^\s*#+\s*test[-_ ]?loopback\s*:", re.IGNORECASE)
#: a listener or socket bound to 127.0.0.1
BINDS_IPV4_LOOPBACK = re.compile(
    r"""["']node["']\s*:\s*["']127\.0\.0\.1["']"""
    r"""|["']127\.0\.0\.1:0["']"""
    r"""|\bbind(?:INET)?\s*\(\s*["']\[?127\.0\.0\.1"""
    r"""|\bFtpServer\s*\([^)]*["']127\.0\.0\.1["']"""
)
#: a client address whose host is localhost: a URL of a scheme that connects to the test's servers, or a host:port
#: string; placeholders of other schemes, file URIs and a bare host name (a bind, a proxy exception) connect nowhere
CONNECTS_LOCALHOST = re.compile(
    r"""["'](?:(?:https?|wss?|rests?|ftps?|e?smtps?|mllp)://(?:[^@/"']*@)?)?localhost[:/]""")

#: tests that another change fixes; each entry names the issue that removes it
KNOWN = {
    # fixed with qore#7462 (umbrella/qore1), which changes this test
    "examples/test/qlib/HttpClientIo/HttpClientIoRedirect.qtest": "qore#7462",
}


def all_tests():
    """The tests that run_tests.sh runs: those of examples/test and of the binary modules"""
    tests = sorted(TESTS.rglob("*.qtest"))
    tests += sorted((ROOT / "modules").glob("*/test/**/*.qtest"))
    return tests


def check(path):
    """Returns a list of the errors of a test"""
    text = path.read_text(errors="replace")
    errors = []
    declared = False
    for number, line in enumerate(text.splitlines(), 1):
        if not ANY_MARKER.match(line):
            continue
        if MARKER.match(line):
            if declared:
                errors.append("%s:%d: more than one test-loopback declaration" % (path, number))
            declared = True
        else:
            errors.append("%s:%d: malformed declaration; expecting '# test-loopback: localhost - <why>'"
                % (path, number))
    if declared:
        return errors
    # comments do not count: only code binds and connects
    code = "\n".join(line for line in text.splitlines() if not line.lstrip().startswith("#"))
    if BINDS_IPV4_LOOPBACK.search(code) and CONNECTS_LOCALHOST.search(code):
        errors.append("%s: listens on 127.0.0.1 but connects to localhost, which can reach a test running beside "
            "it on ::1; connect to 127.0.0.1, or listen on both loopback addresses and declare "
            "'# test-loopback: localhost - <why>'" % path)
    return errors


class LoopbackHostsTest(unittest.TestCase):
    def test_tests(self):
        errors = []
        for path in all_tests():
            rel = str(path.relative_to(ROOT))
            found = check(path)
            if rel in KNOWN:
                if not found:
                    errors.append("%s: no longer needs its entry in KNOWN (%s); remove it" % (rel, KNOWN[rel]))
                continue
            errors += found
        self.assertEqual([], errors, "\n" + "\n".join(errors))

    def test_known_exist(self):
        for rel in KNOWN:
            self.assertTrue((ROOT / rel).is_file(), "%s is listed in KNOWN but does not exist" % rel)

    def test_detects(self):
        # a test that listens on 127.0.0.1 and connects to localhost is found
        self.assertTrue(BINDS_IPV4_LOOPBACK.search('"node": "127.0.0.1",'))
        self.assertTrue(BINDS_IPV4_LOOPBACK.search('s.bind("127.0.0.1:0");'))
        self.assertTrue(BINDS_IPV4_LOOPBACK.search('s.bindINET("127.0.0.1", 0);'))
        self.assertTrue(BINDS_IPV4_LOOPBACK.search('FtpServer(0, verbose, "127.0.0.1")'))
        for url in ('"https://localhost:" + port', '"wss://localhost:" + port', '"localhost:" + port',
                "'http://localhost/path'", '"rests://localhost:" + port', '"ftp://user:pass@localhost:" + port',
                '"esmtp://u:p@localhost:" + port'):
            self.assertTrue(CONNECTS_LOCALHOST.search(url), url)
        # other host names and addresses are not, nor strings that connect nowhere
        for url in ('"https://127.0.0.1:" + port', '"https://localhost.example.com/"', '"mylocalhost:1"',
                '"bfl://localhost"', '"file://localhost/tmp/x"', '"redis://localhost:6379"', '"localhost"'):
            self.assertFalse(CONNECTS_LOCALHOST.search(url), url)

    def test_markers(self):
        self.assertTrue(MARKER.match("# test-loopback: localhost - the TLS host name is checked"))
        for line in ("# test-loopback: localhost", "# test-loopback: localhost -", "# test-loopback: ::1 - why",
                "#test-loopback: localhost - why"):
            self.assertFalse(MARKER.match(line), line)
        self.assertTrue(ANY_MARKER.match("# Test-Loopback: whatever"))


if __name__ == "__main__":
    unittest.main()
