# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The probes of short-string-access.qtest: each makes Qore code return strings of 6 bytes or fewer, which a value can
# hold inline (with no QoreStringNode), to the dataframe module, and prints what the module made of them: the SQL
# names of a table and its columns that the SqlUtil driver module returns (ShortprobeSqlUtil.qm, in the directory of
# this script, for the "shortprobe" driver of SqlRecordingDatasource).

%modern
%requires dataframe

#! the probes in the order that they run
const Probes = (
    # the SQL names returned by SqlUtil for a table that SqlUtil does not see (ex: a temporary table)
    "toTable column names",
);

#! runs a probe and returns its result
string sub run_probe(string name) {
    switch (name) {
        case "toTable column names": {
            DataFrame::DataFrame df({"a": (1,), "in": (2,), "by": (3,)});
            SqlRecordingDatasource ds(False);
            df.toTable(ds, "t");
            return sprintf("%y %y", ds.prepared, ds.executed);
        }
    }
    throw "PROBE-ERROR", sprintf("unknown probe %y", name);
}

foreach string name in (ARGV ?: Probes) {
    try {
        printf("%s: %s\n", name, run_probe(name));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", name, ex.err, ex.desc);
    }
}

#! a statement of a SqlRecordingDatasource that records the SQL it prepares and the arguments it executes it with
class SqlRecordingStatement inherits AbstractSQLStatement {
    private {
        SqlRecordingDatasource ds;
        *string sql;
    }

    constructor(SqlRecordingDatasource ds) {
        self.ds = ds;
    }

    nothing prepare(string sql, ...) {
        self.sql = sql;
        ds.recordPrepare(sql);
    }
    nothing prepareRaw(string sql) {
        self.sql = sql;
        ds.recordPrepare(sql);
    }
    nothing bind(...) {}
    nothing bindArgs(softlist<auto> arguments) {}
    nothing bindPlaceholders(...) {}
    nothing bindPlaceholdersArgs(softlist<auto> arguments) {}
    nothing bindValues(...) {}
    nothing bindValuesArgs(softlist<auto> arguments) {}
    nothing exec(...) {
        ds.recordExec(argv);
    }
    nothing execArgs(softlist<auto> arguments) {
        ds.recordExec(arguments);
    }
    int affectedRows() {
        return 1;
    }
    hash<auto> getOutput() {
        return {};
    }
    hash<auto> getOutputRows() {
        return {};
    }
    nothing define() {}
    nothing close() {}
    nothing commit() {}
    nothing rollback() {}
    nothing beginTransaction() {}
    bool next() {
        return False;
    }
    bool valid() {
        return False;
    }
    *hash<auto> fetchRow() {}
    *hash<auto> getValue() {}
    list<auto> fetchRows(softint rows = -1) {
        return ();
    }
    hash<auto> fetchColumns(softint rows = -1) {
        return {};
    }
    hash<auto> describe() {
        return {};
    }
    *string getSQL() {
        return sql;
    }
    bool active() {
        return False;
    }
    bool currentThreadInTransaction() {
        return False;
    }
    auto memberGate(string key) {}
}

#! an SQLite datasource of the "shortprobe" driver that records the SQL it is given; it has a table "t" only if so
#! constructed (from dataframe.qtest)
class SqlRecordingDatasource inherits AbstractDatasource {
    public {
        #! the SQL executed directly
        list<string> sql = ();
        #! the SQL prepared in statements
        list<string> prepared = ();
        #! the arguments of each execution of a prepared statement
        list<auto> executed = ();
    }

    private {
        bool table_exists;
    }

    constructor(bool table_exists) {
        self.table_exists = table_exists;
    }

    recordPrepare(string sql) {
        push prepared, sql;
    }

    recordExec(*softlist<auto> args) {
        push executed, args;
    }

    nothing commit() {}
    nothing rollback() {}
    auto exec(string sql, ...) {
        push self.sql, sql;
        return 1;
    }
    auto vexec(string sql, *softlist<auto> args) {
        push self.sql, sql;
        return 1;
    }
    auto execRaw(string sql) {
        push self.sql, sql;
        return 1;
    }
    auto select(string sql, ...) {}
    auto selectTyped(string sql, ...) {}
    # SqlUtil checks whether the table exists with one row query
    auto selectRow(string sql, ...) {
        return table_exists ? {"name": "t"} : NOTHING;
    }
    auto selectRows(string sql, ...) {}
    auto selectRowsTyped(string sql, ...) {}
    auto vselect(string sql, *softlist<auto> args) {}
    auto vselectTyped(string sql, *softlist<auto> args) {}
    auto vselectRow(string sql, *softlist<auto> args) {
        return table_exists ? {"name": "t"} : NOTHING;
    }
    auto vselectRows(string sql, *softlist<auto> args) {}
    auto vselectRowsTyped(string sql, *softlist<auto> args) {}
    nothing beginTransaction() {}
    *string getUserName() {}
    *string getPassword() {}
    *string getDBName() {
        return "test";
    }
    *string getDBEncoding() {
        return "UTF-8";
    }
    *string getOSEncoding() {
        return "UTF-8";
    }
    *string getHostName() {}
    *int getPort() {}
    # the driver module is ShortprobeSqlUtil
    string getDriverName() {
        return "shortprobe";
    }
    string getDriverRealName() {
        return "shortprobe";
    }
    int getCapabilities() {
        return 0;
    }
    AbstractSQLStatement getSQLStatement() {
        return new SqlRecordingStatement(self);
    }
    auto getServerVersion() {
        return "3.45.0";
    }
    auto getClientVersion() {
        return "3.45.0";
    }
    bool inTransaction() {
        return False;
    }
    hash<auto> getConfigHash() {
        return {"type": "shortprobe", "db": "test"};
    }
    string getConfigString() {
        return "shortprobe:test";
    }
}

