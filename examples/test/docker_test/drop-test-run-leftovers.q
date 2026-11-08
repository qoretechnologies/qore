#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Drops the databases, schemas and users that killed test runs left on the test database servers of a CI job: each
# run of a database test has objects of its own (the isolation modules in examples/test/qlib/SqlUtil), which it drops
# when it ends, and a run that is killed (a CI job that is cancelled or times out) leaves them.  A CI job's pod has a
# host name and process ids of its own, so an object is a leftover by its age only: at least TestRun::LeftoverAge old,
# whatever the host and process that made it (see TestRunIsolation.qm).  The servers are those of the job's
# environment: QORE_DB_CONNSTR_MYSQL, QORE_DB_CONNSTR_ORACLE_SYS (a user that may drop users),
# QORE_DB_CONNSTR_FREETDS (SQL Server), and QORE_TEST_FIREBIRD_DATA_DIR, a Firebird server's data directory where it
# can be read.  A server that cannot be reached is reported and skipped: the cleanup does not fail the job.
#
# usage: qore drop-test-run-leftovers.q

%modern
%require-types
%strict-args

%prepend-module-path "${SCRIPT_DIR}/../../../qlib"
%requires Util

%prepend-module-path "${SCRIPT_DIR}/../qlib/SqlUtil"
%requires ../qlib/SqlUtil/TestRunIsolation.qm
%requires ../qlib/SqlUtil/MysqlTestIsolation.qm
%requires ../qlib/SqlUtil/OracleTestIsolation.qm
%requires ../qlib/SqlUtil/MssqlTestIsolation.qm
%requires ../qlib/SqlUtil/FirebirdTestIsolation.qm

%exec-class DropTestRunLeftovers

class DropTestRunLeftovers {
    constructor() {
        # the datasource strings are not printed, as they have passwords
        code drop = sub (string label, *string connstr, code dropper) {
            if (!connstr) {
                return;
            }
            try {
                list<string> dropped = dropper(connstr);
                printf("%s: %d leftover test run object(s) dropped%s\n", label, dropped.size(),
                    dropped ? ": " + (foldl $1 + ", " + $2, dropped) : "");
            } catch (hash<ExceptionInfo> ex) {
                # a message about a datasource string quotes it with its password
                printf("%s: skipped: %s: %s\n", label, ex.err, TestRun::maskSecrets(ex.desc ?? "", connstr));
            }
        };
        drop("MySQL", ENV.QORE_DB_CONNSTR_MYSQL, \MysqlRunDatabase::dropLeftovers());
        drop("Oracle", ENV.QORE_DB_CONNSTR_ORACLE_SYS, \OracleRunSchema::dropLeftovers());
        drop("SQL Server", ENV.QORE_DB_CONNSTR_FREETDS, \MssqlRunDatabase::dropLeftovers());
        drop("Firebird files", ENV.QORE_TEST_FIREBIRD_DATA_DIR, \FirebirdRunDatabase::removeLeftoverFiles());
    }
}
