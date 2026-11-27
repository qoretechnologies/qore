#!/usr/bin/env python3
"""Tests write their files under a temporary directory of their own, not at fixed paths in /tmp (qore#7817)

With ``QORE_TEST_TMP_LEAKS``, run_tests.sh gives each test a temporary directory of its own as ``TMPDIR`` and
reports what the test leaves there.  A test that writes to a fixed path in ``/tmp`` bypasses ``TMPDIR``: it leaves
its files in the shared ``/tmp`` when it fails (or always), where nothing reports or removes them, and its names can
collide with a test running beside it.  A test makes its files under ``tmp_location()``, in a ``TmpDir`` or with
``make_tmp_dir()``, which are removed also when it fails.

A test fails this check when its source has a string literal that starts with ``/tmp/``.  The tests that have one
today are listed in ``KNOWN``, which only shrinks: a test in it that no longer has one must be removed from it.  Some
of them use such a path only as data (URL and path parsing tests) and stay listed.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[3]

#: a string literal that starts with /tmp/
FIXED_TMP = re.compile(r"""["']/tmp/""")

#: the sources of tests that have a fixed path in /tmp (qore#7817); remove an entry when its test is converted
KNOWN = frozenset((
    "examples/test/ir/AOTSmoke.qc",
    "examples/test/ir/BgCompileShutdown.qtest",
    "examples/test/ir/IRExecModeSmoke.qtest",
    "examples/test/ir/IRInPlaceListPush.qtest",
    "examples/test/ir/JitCompileThreadStack.qtest",
    "examples/test/ir/benchmark_exec_modes.qr",
    "examples/test/ir/benchmark_execution_speed.qr",
    "examples/test/ir/exec-mode-harness/exec-mode-harness.qtest",
    "examples/test/modules/sshutil/SshUtilRegistrySandbox.qtest",
    "examples/test/modules/sshutil/sshutil.qtest",
    "examples/test/qlib/ConnectionProvider/ConnectionUpStatus.qtest",
    "examples/test/qlib/DataProvider/DebugCollector.qtest",
    "examples/test/qlib/DbDataProvider/DatasourceConnection.qtest",
    "examples/test/qlib/HttpUtil/HttpRedirectFollower.qtest",
    "examples/test/qlib/ImapClient/ImapClient.qtest",
    "examples/test/qlib/JsonFileDataProvider/JsonFileDataProvider.qtest",
    "examples/test/qlib/LlamaParseDataProvider/LlamaParseDataProvider.qtest",
    "examples/test/qlib/Pop3Client/Pop3Client.qtest",
    "examples/test/qlib/QLS/Diagnostics.qtest",
    "examples/test/qlib/QLS/Document.qtest",
    "examples/test/qlib/QLS/WorkspaceIndex.qtest",
    "examples/test/qlib/QoreApiMetadata/QoreApiMetadata.qtest",
    "examples/test/qlib/QoreCodeFormat/qfmt.qtest",
    "examples/test/qlib/QoreHistory/QoreHistory.qtest",
    "examples/test/qlib/RestClientIo/RestClientIo.qtest",
    "examples/test/qlib/SmtpClient/SmtpClient.qtest",
    "examples/test/qlib/SqlUtil/FirebirdTestIsolation.qtest",
    "examples/test/qlib/SqlUtil/TestRunPrecondition.qtest",
    "examples/test/qlib/TelnetClient/TelnetClient.qtest",
    "examples/test/qlib/UnstructuredDataProvider/UnstructuredDataProvider.qtest",
    "examples/test/qlib/UrlDownloader/UrlDownloader.qtest",
    "examples/test/qore/classes/HTTPClient/HttpClientIpv6Url.qtest",
    "examples/test/qore/classes/Program/parse-options.qtest",
    "examples/test/qore/closures/const-deferred-closure-init.qtest",
    "examples/test/qore/functions/parse_datasource.qtest",
    "examples/test/qore/functions/parse_url.qtest",
    "examples/test/qore/parser/module-path-directive.qtest",
    "examples/test/qore/security/path-nul.qtest",
    "examples/test/qore/security/sandbox-entry-points.qtest",
    "examples/test/qore/security/sandbox-manager.qtest",
    "examples/test/qore/vars/named-arguments.qtest",
    "examples/test/qore/vars/timezone-spelling-cache.qtest",
    "modules/krb5/test/krb5.qtest",
    "modules/linenoise/test/linenoise-sandbox.qtest",
))


def sources():
    roots = [ROOT / "examples/test"] + sorted((ROOT / "modules").glob("*/test"))
    for root in roots:
        for path in sorted(root.rglob("*")):
            if path.is_file() and path.suffix in (".qtest", ".q", ".qm", ".qc", ".qr", ".ql"):
                yield path


def offenders():
    found = set()
    for path in sources():
        if FIXED_TMP.search(path.read_text(errors="replace")):
            found.add(path.relative_to(ROOT).as_posix())
    return found


class FixedTmpPathTest(unittest.TestCase):
    def test_no_new_fixed_tmp_paths(self):
        new = sorted(offenders() - KNOWN)
        self.assertEqual([], new, "a test writes to a fixed path in /tmp: use tmp_location(), TmpDir or "
                         "make_tmp_dir() (qore#7817)")

    def test_known_list_shrinks(self):
        fixed = sorted(KNOWN - offenders())
        self.assertEqual([], fixed, "remove these tests from KNOWN: they no longer have a fixed path in /tmp")

    def test_runner_gives_tests_their_own_tmpdir(self):
        runner = (ROOT / "run_tests.sh").read_text()
        self.assertIn('TEST_ENV="$TEST_ENV TMPDIR=$TEST_TMPDIR"', runner)
        self.assertIn("check_test_tmpdir", runner)


if __name__ == "__main__":
    unittest.main()
