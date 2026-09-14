#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Child process for PgsqlUpsertDiagnostics.qtest
# Copyright 2026 Qore Technologies, s.r.o.
#
# Exercises rejected native PostgreSQL upserts, lost connections, and SQL callback errors with the SqlUtil modules
# loaded either from source (-DSourceModules) or from their AOT-compiled qmods.  The probe never writes to its own
# stdout or stderr: all results, including unexpected probe errors, are serialized to the file given as the first
# argument, so the parent can require empty process output.
#
# Environment:
#   QORE_DB_CONNSTR_PGSQL: PostgreSQL connection string
#   QORE_TEST_PGSQL_DIAG_TABLE: name of the table to create and drop
#   QORE_TEST_PGSQL_DIAG_KEY: key value rejected by a check constraint
#   QORE_TEST_PGSQL_DIAG_SECRET: synthetic value that must never reach the process output

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../qlib"
%ifdef SourceModules
%requires ../../../../qlib/SqlUtil
%requires ../../../../qlib/PgsqlSqlUtilBase.qm
%requires ../../../../qlib/PgsqlSqlUtil.qm
%else
%requires ../../../../qlib/SqlUtil/SqlUtil.qmod
%requires ../../../../qlib/PgsqlSqlUtilBase.qmod
%requires ../../../../qlib/PgsqlSqlUtil.qmod
%endif

%exec-class PgsqlUpsertDiagnosticsProbe

class PgsqlUpsertDiagnosticsProbe {
    private {
        Datasource ds;
        Datasource admin;
        string table_name;
        string rejected_key;
        string secret;

        hash<auto> result = {};

        #! milliseconds to wait for a terminated backend to exit
        const TerminateTimeout = 60000;
    }

    constructor() {
        try {
            run();
        } catch (hash<ExceptionInfo> ex) {
            result.probe_error = PgsqlUpsertDiagnosticsProbe::describe(ex) + {"location": get_ex_pos(ex)};
        }
        File f();
        f.open2(ARGV[0], O_CREAT | O_WRONLY | O_TRUNC, 0600);
        f.write(Serializable::serialize(result));
    }

    private run() {
        table_name = ENV.QORE_TEST_PGSQL_DIAG_TABLE;
        rejected_key = ENV.QORE_TEST_PGSQL_DIAG_KEY;
        secret = ENV.QORE_TEST_PGSQL_DIAG_SECRET;

        result.modules = map {$1: get_module_hash(){$1}.filename}, ("SqlUtil", "PgsqlSqlUtilBase", "PgsqlSqlUtil");

        ds = new Datasource(ENV.QORE_DB_CONNSTR_PGSQL);
        admin = new Datasource(ENV.QORE_DB_CONNSTR_PGSQL);

        admin.exec(sprintf("create table %s (keyname varchar(200) primary key, value varchar(200) not null)",
            table_name));
        admin.commit();
        on_exit {
            # the probe connection must release its locks before the table can be dropped
            try {
                ds.rollback();
            } catch (hash<ExceptionInfo> ex) {
                result.cleanup_error = PgsqlUpsertDiagnosticsProbe::describe(ex);
            }
            admin.exec(sprintf("drop table if exists %s", table_name));
            admin.commit();
        }

        admin.exec(sprintf("alter table %s add constraint %s_reject check (keyname <> '%s')", table_name, table_name,
            rejected_key));
        admin.commit();

        Table table(ds, table_name);
        AbstractTable t = table.getTable();
        result.atomic_upsert = cast<PgsqlTable>(t).hasAtomicUpsert();

        # an accepted upsert caches the server version and the upsert closure, so the failures below are raised
        # while the closure executes its statement
        t.upsert({"keyname": "accepted", "value": "accepted"});
        ds.commit();

        result.constraint = constraintViolation(t);
        if (ds.getServerVersion() >= 140000) {
            result.connection_loss = connectionLoss(t);
        } else {
            result.connection_loss_skipped = sprintf("PostgreSQL server version %d does not support "
                "pg_terminate_backend() with a timeout", ds.getServerVersion());
        }
        result.sql_callback = sqlCallbackError(t);
    }

    private hash<auto> constraintViolation(AbstractTable t) {
        hash<auto> rv = {};

        *hash<ExceptionInfo> raw = PgsqlUpsertDiagnosticsProbe::getException(sub () {
            ds.vexec(sprintf("insert into %s (keyname, value) values (%%v, %%v)", table_name),
                (rejected_key, secret));
        });
        ds.rollback();
        if (!raw) {
            throw "PROBE-ERROR", "the check constraint did not reject the direct insert";
        }
        rv.raw = PgsqlUpsertDiagnosticsProbe::describe(raw);

        *hash<ExceptionInfo> ex = PgsqlUpsertDiagnosticsProbe::getException(sub () {
            t.upsert({"keyname": rejected_key, "value": secret});
        });
        ds.rollback();
        if (!ex) {
            throw "PROBE-ERROR", "the check constraint did not reject the native upsert";
        }
        rv.upsert = PgsqlUpsertDiagnosticsProbe::describe(ex);
        rv.restart_reason = PgsqlUpsertDiagnosticsProbe::getRestartReason(ex);
        return rv;
    }

    private hash<auto> connectionLoss(AbstractTable t) {
        hash<auto> rv = {};

        terminateBackend();
        *hash<ExceptionInfo> raw = PgsqlUpsertDiagnosticsProbe::getException(sub () {
            ds.vexec(sprintf("insert into %s (keyname, value) values (%%v, %%v)", table_name),
                ("raw-after-loss", secret));
        });
        ds.rollback();
        if (!raw) {
            throw "PROBE-ERROR", "the direct insert succeeded on a terminated connection";
        }
        rv.raw = PgsqlUpsertDiagnosticsProbe::describe(raw);
        rv.raw_restart_reason = PgsqlUpsertDiagnosticsProbe::getRestartReason(raw);

        terminateBackend();
        *hash<ExceptionInfo> ex = PgsqlUpsertDiagnosticsProbe::getException(sub () {
            t.upsert({"keyname": "upsert-after-loss", "value": secret});
        });
        ds.rollback();
        if (!ex) {
            throw "PROBE-ERROR", "the native upsert succeeded on a terminated connection";
        }
        rv.upsert = PgsqlUpsertDiagnosticsProbe::describe(ex);
        rv.restart_reason = PgsqlUpsertDiagnosticsProbe::getRestartReason(ex);
        return rv;
    }

    private hash<auto> sqlCallbackError(AbstractTable t) {
        *string callback_sql;
        code<nothing(string)> sql_callback = sub (string sql) {
            throw "SQL-CALLBACK-REJECTED", "the SQL callback rejected the statement";
        };
        code<nothing(int, string, string, *string, *string, *string, string, hash<ExceptionInfo>)> error_callback =
                sub (int ac, string type, string name, *string table, *string new_name, *string info, string sql,
                    hash<ExceptionInfo> ex) {
            callback_sql = sql;
        };

        *hash<ExceptionInfo> ex = PgsqlUpsertDiagnosticsProbe::getException(sub () {
            t.getAddColumnSql("diagnostic_extra", {"qore_type": Type::String, "size": 200, "default_value": secret},
                True, {"sql_callback": sql_callback, "error_callback": error_callback});
        });
        if (!ex) {
            throw "PROBE-ERROR", "the SQL callback exception was not raised";
        }
        return {
            "exception": PgsqlUpsertDiagnosticsProbe::describe(ex),
            "callback_sql": callback_sql,
        };
    }

    #! terminates the probe connection's backend inside a transaction and waits until it has exited
    private terminateBackend() {
        ds.beginTransaction();
        int pid = ds.selectRow("select pg_backend_pid() as pid").pid;
        on_exit admin.commit();
        if (!admin.selectRow("select pg_terminate_backend(%v, %v) as terminated", pid, TerminateTimeout).terminated) {
            throw "PROBE-ERROR", sprintf("PostgreSQL backend %d did not exit within %dms", pid, TerminateTimeout);
        }
    }

    private static *hash<ExceptionInfo> getException(code<nothing()> c) {
        try {
            c();
        } catch (hash<ExceptionInfo> ex) {
            return ex;
        }
    }

    private static *string getRestartReason(hash<ExceptionInfo> ex) {
        RestartableTransaction policy("pgsql", 1, 0s, 1);
        return policy.restartTransaction(ex);
    }

    private static hash<auto> describe(hash<ExceptionInfo> ex) {
        list<hash<auto>> chain = ();
        for (*hash<ExceptionInfo> e = ex.next; e; e = e.next) {
            chain += {"err": e.err, "desc": e.desc, "arg": e.arg};
        }
        return {
            "err": ex.err,
            "desc": ex.desc,
            "arg": ex.arg,
            "callstack_type": ex.callstack[0].type,
            "next": chain,
        };
    }
}
