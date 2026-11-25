# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the trusted host of sqlutil-caller-database.qtest: it loads SqlUtil and DbDataProvider, then runs each case in a
# Program of its own (restricted with PO_NO_DATABASE or not, with a sandbox or not) and prints "<case>=<outcome>":
# "ok" if the call returned, else the code of the exception.  An allowed call opens a datasource of a driver that does
# not exist, so it fails with LOAD-MODULE-ERROR when the connection is used; a refused call raises PERMISSION-DENIED
# (module code reached for the caller, qore#6257), INVALID-FUNCTION-ACCESS (a call of a variant in the DATABASE domain
# made at run time) or a PARSE-EXCEPTION for the DATABASE domain (a direct call); a subclass that a sandboxed case
# declares cannot use the private members of the trusted connection class (qore#6843: PARSE-EXCEPTION-PRIVATE)
%modern
%requires SqlUtil
%requires PgsqlSqlUtilBase
%requires DbDataProvider
%requires CallerDbOuter

#! the connection string of a driver that does not exist
const Conn = "qore6257nodriver:u/p@db";

#! a datasource configuration for the same driver
const ConnHash = {"type": "qore6257nodriver", "user": "u", "pass": "p", "db": "db"};

#! the code that every case Program gets before its own
const Prefix = "%requires SqlUtil
%requires PgsqlSqlUtilBase
%requires DbDataProvider
%requires CallerDbOuter
const Conn = \"qore6257nodriver:u/p@db\";
const ConnHash = {\"type\": \"qore6257nodriver\", \"user\": \"u\", \"pass\": \"p\", \"db\": \"db\"};
# the outcome of a call
string sub outcome(code c) {
    try {
        c();
        return \"ok\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
# serialized data of a table whose datasource is deserialized from the connection string
hash<SerializationInfo> sub table_data() {
    return <SerializationInfo>{
        \"_index\": {
            \"0\": <ObjectSerializationInfo>{\"_class\": \"PgsqlSqlUtilBase::PgsqlTable\",
                \"_class_data\": {\"SqlUtil::AbstractSqlUtilBase\": {\"ds\": {\"_index\": \"1\"}}}},
            \"1\": <HashSerializationInfo>{\"_hash\": \"^string\",
                \"_members\": {\"ds_type\": \"Datasource\", \"ds\": Conn}},
        },
        \"_data\": {\"_index\": \"0\"},
    };
}
";

#! the calls that open a datasource from a connection string or configuration, made directly
const DirectCalls = {
    "getDatabase": "SqlUtil::AbstractDatabase db = SqlUtil::AbstractDatabase::getDatabase(Conn); "
        "db.getDatasource().open()",
    "getDatabase-hash": "SqlUtil::AbstractDatabase db = SqlUtil::AbstractDatabase::getDatabase(ConnHash); "
        "db.getDatasource().open()",
    "getTable": "SqlUtil::AbstractTable::getTable(Conn, \"t\")",
    "getTable-hash": "SqlUtil::AbstractTable::getTable(ConnHash, \"t\")",
    "Table": "new SqlUtil::Table(Conn, \"t\")",
    "Table-hash": "new SqlUtil::Table(ConnHash, \"t\")",
    "Database": "new SqlUtil::Database(Conn)",
    "Database-hash": "new SqlUtil::Database(ConnHash)",
};

#! the calls that reach the check in module code for the caller
const ModuleCalls = {
    # DbDataProvider gives a connection string to the datasource itself
    "dbdataprovider": "AbstractDatasource ds = DbDataProvider::DbDataProviderBase::getDatasource(Conn); "
        "ds.beginTransaction()",
    # SqlUtil deserializes a table's datasource from its connection string
    "deserialize": "Serializable::deserialize(table_data())",
    # a module whose code opens a table through the variant in the DATABASE domain, called through another module
    "module-chain": "CallerDbOuter::open_table(Conn)",
    # a connection that the caller creates from a URL, which opens a datasource when it is used (qore#6385)
    "connection": "DbDataProvider::DatasourceConnection c(\"c\", \"test\", \"db://\" + Conn); c.get()",
    # a connection that the caller deserializes, which gives its URL as well (qore#6385)
    "deserialize-connection": "DbDataProvider::DatasourceConnection c = Serializable::deserialize("
        "host_connection_data()); c.get()",
};

#! a function of the host that calls back code that the case gives it
auto sub host_callback(code c) {
    return c();
}

#! a connection that the host creates: the code that it gives it to may use it (qore#6385)
DbDataProvider::DatasourceConnection host_connection("host", "test", "db://" + Conn);

#! returns the connection that the host created
DbDataProvider::DatasourceConnection sub host_get_connection() {
    return host_connection;
}

#! returns the serialized data of the connection that the host created
hash<SerializationInfo> sub host_connection_data() {
    return host_connection.serializeToData();
}

#! a subclass of the connection that replaces the setup of the base class (qore#6385)
const SubclassCase = "class X inherits DbDataProvider::DatasourceConnection {
    constructor() : DbDataProvider::DatasourceConnection(\"x\", \"test\", \"db://\" + Conn) {
    }
    private commonSetup() {
        ds_urlh = parse_datasource(Conn);
    }
}
string sub t() { return outcome(sub () { X x(); x.get(); }); }";

#! a subclass of the connection that the case deserializes from data without the connection's own members, then gives
#! its URL itself (qore#6385)
const SubclassDeserializeCase = "class Y inherits DbDataProvider::DatasourceConnection {
    constructor() : DbDataProvider::DatasourceConnection(\"y\", \"test\", \"db://\" + Conn) {
    }
    setUrl() {
        ds_urlh = parse_datasource(Conn);
    }
}
string sub t() {
    return outcome(sub () {
        Y y = Serializable::deserialize(<SerializationInfo>{
            \"_index\": {\"0\": <ObjectSerializationInfo>{\"_class\": \"Y\", \"_class_data\": {
                \"ConnectionProvider::AbstractConnection\": {\"name\": \"y\"},
            }}},
            \"_data\": {\"_index\": \"0\"},
        });
        y.setUrl();
        y.get();
    });
}";

#! a function of the host that opens a table for the code that calls it: the host's code decides, not its caller's
sub host_open_table(string conn) {
    AbstractTable::getTable(conn, "t");
}

#! the execution modes that the test gives for the case Programs
const ExecModes = {"ast": EM_AST, "ir": EM_IR, "jit": EM_JIT, "tiered": EM_TIERED};

#! runs a case: returns the outcome of t() in a new Program, or the code of the exception of its parse
/** The Program runs in the execution mode of SQLUTIL_CALLER_DATABASE_MODE, the mode of the worker's own run
*/
string sub run_case(int po, bool sandbox, string code) {
    Program p(PO_MODERN | po);
    if (*string mode = ENV.SQLUTIL_CALLER_DATABASE_MODE) {
        p.setExecMode(ExecModes{mode});
    }
    if (sandbox) {
        p.setSandboxManager(new SandboxManager());
    }
    p.importFunction("host_callback");
    p.importFunction("host_open_table");
    p.importFunction("host_get_connection");
    p.importFunction("host_connection_data");
    try {
        p.parse(Prefix + code, "case");
    } catch (hash<ExceptionInfo> ex) {
        # a direct call of a variant in the DATABASE domain is refused when it is parsed
        if (ex.err == "PARSE-EXCEPTION" && ex.desc =~ /functional domain: DATABASE/) {
            return "PARSE-EXCEPTION-DATABASE";
        }
        # a class declared by sandboxed code cannot use the private declarations of a trusted class (qore#6843)
        if (ex.err == "PARSE-EXCEPTION" && ex.desc =~ /a class declared by sandboxed code cannot use the private/) {
            return "PARSE-EXCEPTION-PRIVATE";
        }
        return sprintf("%s: %s", ex.err, ex.desc);
    }
    return p.callFunction("t");
}

sub result(string key, string value) {
    printf("%s=%s\n", key, value);
}

# the callers: unrestricted, unrestricted with a sandbox (a trusted Qorus interface), and restricted with
# PO_NO_DATABASE (a sandboxed Qorus interface, whose options are built from PO_LOCKDOWN), with and without a sandbox
const Callers = (
    ("name": "trusted", "po": 0, "sandbox": False),
    ("name": "trusted-sandboxed", "po": 0, "sandbox": True),
    ("name": "restricted", "po": PO_NO_DATABASE, "sandbox": False),
    ("name": "restricted-sandboxed", "po": PO_NO_DATABASE, "sandbox": True),
);

# the execution mode of the case Programs, as the test gives it
if (*string mode = ENV.SQLUTIL_CALLER_DATABASE_MODE) {
    Program p(PO_MODERN);
    p.setExecMode(ExecModes{mode});
    result("case-mode", p.getExecMode() == ExecModes{mode} ? mode : sprintf("%y", p.getExecMode()));
}

foreach hash<auto> caller in (Callers) {
    string n = caller.name;
    foreach hash<auto> i in (DirectCalls.pairIterator()) {
        # a direct call
        result(sprintf("%s.direct.%s", n, i.key), run_case(caller.po, caller.sandbox,
            sprintf("string sub t() { return outcome(sub () { %s; }); }", i.value)));
    }
    # a function of the trusted host that opens the table for the caller
    result(n + ".host-function", run_case(caller.po, caller.sandbox,
        "string sub t() { return outcome(sub () { host_open_table(Conn); }); }"));
    # a subclass of the connection that replaces the setup of the base class
    result(n + ".subclass-connection", run_case(caller.po, caller.sandbox, SubclassCase));
    result(n + ".subclass-deserialize-connection", run_case(caller.po, caller.sandbox, SubclassDeserializeCase));
    # a connection that the host created, which the case opens
    result(n + ".host-connection", run_case(caller.po, caller.sandbox,
        "string sub t() { return outcome(sub () { host_get_connection().get(); }); }"));
    # a call at run time, which the parse cannot see
    result(n + ".runtime.getDatabase", run_case(caller.po, caller.sandbox,
        "string sub t() { return outcome(sub () { call_static_method(\"SqlUtil::AbstractDatabase\", \"getDatabase\", "
        "Conn); }); }"));
    foreach hash<auto> i in (ModuleCalls.pairIterator()) {
        # through module code
        result(sprintf("%s.module.%s", n, i.key), run_case(caller.po, caller.sandbox,
            sprintf("string sub t() { return outcome(sub () { %s; }); }", i.value)));
        # through a closure that the trusted host calls back
        result(sprintf("%s.callback.%s", n, i.key), run_case(caller.po, caller.sandbox,
            sprintf("string sub t() { return outcome(sub () { host_callback(sub () { %s; }); }); }", i.value)));
    }
}

# the host itself, which no restriction governs
hash<string, code> host_calls = {
    "getDatabase": sub () { AbstractDatabase db = AbstractDatabase::getDatabase(Conn); db.getDatasource().open(); },
    "dbdataprovider": sub () {
        AbstractDatasource ds = DbDataProvider::DbDataProviderBase::getDatasource(Conn);
        ds.beginTransaction();
    },
};
foreach hash<auto> i in (host_calls.pairIterator()) {
    string r = "ok";
    try {
        i.value();
    } catch (hash<ExceptionInfo> ex) {
        r = ex.err;
    }
    result("host." + i.key, r);
}
