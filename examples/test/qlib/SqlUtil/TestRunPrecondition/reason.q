#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# run by TestRunPrecondition.qtest with QORE_DB_CONNSTR_PRECOND_TEST and QORE_DB_CONNSTR_PRECOND_OTHER in its
# environment: prints the reason of a failed connection to the first one's database, the variable found for each
# datasource string, and the reason that failUnreachable() raises

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../../qlib"
%requires ../TestRunIsolation.qm

hash<ExceptionInfo> ex = <ExceptionInfo>{
    "err": "DBI:TEST:CONNECT-ERROR",
    "desc": "preuser7@predb7 (prepass7) failed; the admin othuser7@othdb7 (othpass7) too",
    "type": "System",
};
string connstr = ENV.QORE_DB_CONNSTR_PRECOND_TEST;
printf("reason: %s\n", TestRun::getPreconditionFailure("QORE_DB_CONNSTR_PRECOND_TEST", connstr, ex));
printf("variable: %s\n", TestRun::getVariable(connstr));
printf("other: %s\n", TestRun::getVariable(ENV.QORE_DB_CONNSTR_PRECOND_OTHER));
printf("none: %s\n", TestRun::getVariable("pgsql:x/y@z%nowhere"));
try {
    TestRun::failUnreachable(connstr, ex);
} catch (hash<ExceptionInfo> fex) {
    printf("raised: %s: %s\n", fex.err, fex.desc);
}
