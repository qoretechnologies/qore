#!/usr/bin/env python3
"""The Oracle setup of the CI jobs (init_oracle.sh) gives the tests a user that may create the user of each test run.

OracleTestIsolation.qm (examples/test/qlib/SqlUtil) creates a user for each run of an Oracle test, with the
connection of QORE_DB_CONNSTR_ORACLE_SYS, grants it the privileges to create objects in its schema, and drops it with
everything in it; OracleTestIsolation.qtest checks that against a database.  init_oracle.sh writes the variables of
the job to /tmp/env.sh and changes the Oracle client's configuration, so this checks what it writes: with an Oracle
container, the variable names SYSTEM, which has those privileges, and not PDBADMIN, whose PDB_DBA role need not have
them; without one, no Oracle variable is written, so the Oracle tests are skipped (the Oracle server rippy that jobs
without a container used is retired).

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "examples/test/docker_test/init_oracle.sh"


class CiOracleSetupTest(unittest.TestCase):
    def test_admin_connection(self):
        source = SCRIPT.read_text()
        container, other = source.split("\nelse\n", 1)
        self.assertIn('if [ -n "$DOCKER_ORACLE" ]; then', container)
        exports = re.compile(r"echo export QORE_DB_CONNSTR_ORACLE_SYS=(\S+) >> /tmp/env.sh")
        self.assertEqual(["oracle:system/qore@oracle"], exports.findall(container))
        # a job without an Oracle container sets no Oracle variable
        self.assertNotIn("QORE_DB_CONNSTR_ORACLE", other)
        self.assertNotIn("/tmp/env.sh", other)
        self.assertNotIn("rippy", source)

if __name__ == "__main__":
    unittest.main()
