#!/usr/bin/env python3
"""The Oracle setup of the CI jobs (init_oracle.sh) gives the tests a user that may create the user of each test run.

OracleTestIsolation.qm (examples/test/qlib/SqlUtil) creates a user for each run of an Oracle test, with the
connection of QORE_DB_CONNSTR_ORACLE_SYS, grants it the privileges to create objects in its schema, and drops it with
everything in it; OracleTestIsolation.qtest checks that against a database.  init_oracle.sh writes the variables of
the job to /tmp/env.sh and changes the Oracle client's configuration, so this checks what it writes: in both of its
setups (an Oracle container, or the Oracle server rippy), the variable names SYSTEM, which has those privileges, and
not PDBADMIN, whose PDB_DBA role need not have them.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "examples/test/docker_test/init_oracle.sh"


class CiOracleSetupTest(unittest.TestCase):
    def test_admin_connection_in_both_setups(self):
        source = SCRIPT.read_text()
        container, rippy = source.split("\nelse\n", 1)
        self.assertIn('if [ -n "$DOCKER_ORACLE" ]; then', container)
        exports = re.compile(r"echo export QORE_DB_CONNSTR_ORACLE_SYS=(\S+) >> /tmp/env.sh")
        for name, part, host in (("container", container, "@oracle"), ("rippy", rippy, "@rippy%rippy:1521")):
            with self.subTest(setup=name):
                values = exports.findall(part)
                self.assertEqual(["oracle:system/qore" + host], values)
        # rippy's admin connection is given only once its test user is created, as the job then has an Oracle
        # database; the variables are removed with the others when it has none
        created = rippy.index('echo "created oracle user ${ORACLE_USER}"')
        self.assertLess(created, rippy.index("QORE_DB_CONNSTR_ORACLE_SYS="))
        self.assertLess(rippy.index("QORE_DB_CONNSTR_ORACLE_SYS="), rippy.index("else\n        echo \"WARNING"))
        self.assertIn("/QORE_DB_CONNSTR_ORACLE/d", rippy)


if __name__ == "__main__":
    unittest.main()
