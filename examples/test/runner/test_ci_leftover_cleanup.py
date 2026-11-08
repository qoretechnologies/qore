#!/usr/bin/env python3
"""The Linux CI test jobs drop the database objects that killed test runs left on the shared test database servers.

Each run of a database test has a database, schema or user of its own (the isolation modules in
examples/test/qlib/SqlUtil), which a run that is killed leaves behind; drop-test-run-leftovers.q drops those that are
old enough, whatever the host that made them (a CI job's pod has a host name and process ids of its own).  The
script's work is checked against the databases by the isolation modules' tests; this checks that the jobs run it after
their tests, as the tests' user, and that it cannot fail the job.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[3]


class CiLeftoverCleanupTest(unittest.TestCase):
    def test_jobs_drop_leftovers_after_the_tests(self):
        for script in ("test-ubuntu.sh", "test-alpine.sh"):
            with self.subTest(script=script):
                source = (ROOT / "examples/test/docker_test" / script).read_text()
                tests = source.index("./run_tests.sh || TEST_RC=$?")
                cleanup = source.index("examples/test/docker_test/drop-test-run-leftovers.q || true")
                self.assertLess(tests, cleanup)
                # as the tests' user, before the job ends on a failure of the tests
                self.assertIn("gosu qore:qore env", source[source.rindex("\n\n", 0, cleanup):cleanup])
                self.assertLess(cleanup, source.index('if [ "${TEST_RC}" != "0" ]; then'))

    def test_script_covers_every_shared_server(self):
        source = (ROOT / "examples/test/docker_test/drop-test-run-leftovers.q").read_text()
        for call in ("MysqlRunDatabase::dropLeftovers", "OracleRunSchema::dropLeftovers",
                     "MssqlRunDatabase::dropLeftovers", "FirebirdRunDatabase::removeLeftoverFiles"):
            self.assertIn(call, source)
        # the datasource strings, which have passwords, are not printed
        self.assertNotRegex(source, r'printf\([^;]*connstr')


if __name__ == "__main__":
    unittest.main()
