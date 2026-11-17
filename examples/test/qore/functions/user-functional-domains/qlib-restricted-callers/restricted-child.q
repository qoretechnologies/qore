#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# the child process of qlib-restricted-callers.qtest: runs one case in a restricted Program (qore#6337)

# Copyright (C) 2026 Qore Technologies, s.r.o.

# args: <case> <exec mode of the restricted Program> <1: the restricted Program has a SandboxManager> [case argument]
# prints the lines that the case's code returns; each case runs in a process of its own, which loads the modules
# freshly

%modern

#! the restrictions of a Qorus interface Program
const Restrictions = PO_NO_PROCESS_CONTROL | PO_NO_THREAD_CONTROL;

const Modes = {"ast": EM_AST, "ir": EM_IR, "jit": EM_JIT};

#! code shared by the restricted code of every case
const Common = "
# the result of a call: its value or the error that it raised
string sub attempt(code c) {
    try {
        auto v = c();
        return v.typeCode() == NT_OBJECT ? v.className() : sprintf(\"%y\", v);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# the result of a call: its value or the error that it raised with its description
string sub attempt_desc(code c) {
    try {
        return sprintf(\"%y\", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err + \": \" + ex.desc;
    }
}

string sub action_names(*hash<auto> h) {
    return h ? sort(keys h).join(\",\") : \"-\";
}
";

#! the start of the unrestricted code of the SqlUtil cases: creates the tables in the SQLite database given
const SqlTrustedPre = "%requires SqlUtil
list<string> sub run(*string arg) {
    Datasource ds(\"sqlite3:@\" + arg);
    ds.exec(\"create table rc_t1 (id integer, name varchar(20))\");
    ds.exec(\"create table rc_t2 (id integer, val varchar(20))\");
    ds.exec(\"create table rc_t3 (id integer)\");
    ds.commit();";

#! the restricted code of the SqlUtil cases: SQL generation and lookups that resolve tables on a cache miss
const SqlRestricted = "%requires SqlUtil
list<string> sub run(*string arg) {
    Datasource ds(\"sqlite3:@\" + arg);
    Table t(ds, \"rc_t1\");
    list<string> l = ();
    l += \"select: \" + attempt_desc(auto sub () {
        string sql = t.getSelectSql({
            \"columns\": (\"id\", \"name\", cop_as(\"b.val\", \"val\")),
            \"join\": join_inner(\"rc_t2\", \"b\", {\"id\": \"id\"}),
        });
        return (sql =~ /join rc_t2 b/i) ? \"joined rc_t2\" : sql;
    });
    # the resolver cached the table
    l += \"cached: \" + attempt(auto sub () { return sqlutil_get_cached_table(ds, \"rc_t2\").getName(); });
    # a table that is not cached yet
    l += \"not cached: \" + attempt(auto sub () { return sqlutil_get_cached_table(ds, \"rc_t3\").getName(); });
    ds.rollback();
    return l;
}";

#! the cases: the code that runs in the restricted Program, and the code that runs in an unrestricted Program before
#! and after it; each defines list<string> sub run(*string arg)
const Cases = {
    # writes a data provider index that names the modules of a factory, an app and a connection scheme; unrestricted
    "build-index": {
        "trusted_pre": "%requires ProviderIndexUtil
list<string> sub run(*string arg) {
    ProviderIndexUtil::writeDataProviderIndex(<DataProviderIndexInfo>{
        \"factorymap\": {\"db\": \"DbDataProvider\"},
        \"appmap\": {\"QoreTestRcIndexApp\": {\"RcIndexMod\": True}},
        \"schememap\": {\"qoretestrcidxscheme\": {\"RcIndexMod\": True}},
    }, 0s, {});
    return (\"index written\",);
}",
    },

    # lookups that load the modules that the data provider index names
    "index": {
        "restricted": "%requires DataProvider
%requires ConnectionProvider
list<string> sub run(*string arg) {
    list<string> l = ();
    l += \"db module before: \" + sprintf(\"%y\", exists get_module_hash().DbDataProvider);
    l += \"factory db: \" + attempt(auto sub () { return DataProvider::getFactory(\"db\"); });
    l += \"db module after: \" + sprintf(\"%y\", exists get_module_hash().DbDataProvider);
    l += \"index module before: \" + sprintf(\"%y\", exists get_module_hash().RcIndexMod);
    l += \"app: \" + attempt(auto sub () { return DataProviderActionCatalog::getApp(\"QoreTestRcIndexApp\").name; });
    l += \"scheme: \" + attempt(auto sub () {
        return ConnectionSchemeCache::getScheme(\"qoretestrcidxscheme\").display_name; });
    l += \"index module after: \" + sprintf(\"%y\", exists get_module_hash().RcIndexMod);
    return l;
}",
    },

    # DPQL resolution of an app action loads the known factory modules
    "dpql": {
        "restricted": "%requires DataProvider
list<string> sub run(*string arg) {
    list<string> l = ();
    l += \"db module before: \" + sprintf(\"%y\", exists get_module_hash().DbDataProvider);
    DpqlActionSession s();
    l += \"set context: \" + attempt_desc(auto sub () {
        return s.handleAction(\"dpql-set-context\", {\"provider\": \"@QoreTestRcNoSuchApp/action\"}); });
    l += \"db module after: \" + sprintf(\"%y\", exists get_module_hash().DbDataProvider);
    return l;
}",
    },

    # a lookup triggers the load of a module that registers a file API and fails
    "file-api": {
        "restricted": "%requires DataProvider
list<string> sub run(*string arg) {
    list<string> l = ();
    l += \"factory: \" + attempt(auto sub () { return DataProvider::getFactory(\"qoretestrcfail\"); });
    l += \"file api: \" + attempt(auto sub () { return exists DataProviderActionCatalog::getAppFileApi(\"QoreTestRcFailApp\"); });
    l += \"file api names: \" + attempt(auto sub () {
        return (DataProviderActionCatalog::getAppFileApiNames() ?? ()).contains(\"QoreTestRcFailApp\"); });
    # a second lookup fails the same way
    l += \"factory again: \" + attempt(auto sub () { return DataProvider::getFactory(\"qoretestrcfail\"); });
    l += \"file api again: \" + attempt(auto sub () {
        return exists DataProviderActionCatalog::getAppFileApi(\"QoreTestRcFailApp\"); });
    return l;
}",
        "trusted_post": "%requires DataProvider
list<string> sub run(*string arg) {
    return (\"trusted file api: \" + sprintf(\"%y\", exists DataProviderActionCatalog::getAppFileApi(\"QoreTestRcFailApp\")),);
}",
    },

    # SQL generation resolves a joined table through the table resolver that an unrestricted Program registered
    "sqlutil": {
        "trusted_pre": SqlTrustedPre + "
    sqlutil_register_table_resolver(\\sqlutil_get_cached_table());
    return (\"resolver registered\",);
}",
        "restricted": SqlRestricted,
    },

    # SQL generation resolves a joined table through the table resolver that a library module registered in its
    # initialization
    "sqlutil-module": {
        "trusted_pre": "%requires RcResolverMod\n" + SqlTrustedPre + "
    return (\"resolver registered by module: \" + (sqlutil_get_table_resolver() ? \"True\" : \"False\"),);
}",
        "restricted": SqlRestricted,
    },

    # deferred registrations that the initialization of a library module made run for a restricted reader
    "lazy": {
        "restricted": "%requires DataProvider
%requires ConnectionProvider
%requires RcLazyMod
list<string> sub run(*string arg) {
    list<string> l = ();
    l += \"lazy runs before: \" + getCalls(\"lazy\");
    l += \"lazy actions: \" + attempt_desc(auto sub () { return action_names(DataProviderActionCatalog::getActionHash(LazyApp)); });
    l += \"lazy runs after: \" + getCalls(\"lazy\");
    l += \"pending runs before: \" + getCalls(\"pending\");
    l += \"pending app: \" + attempt_desc(auto sub () { return DataProviderActionCatalog::getApp(PendingApp).name; });
    l += \"pending actions: \" + attempt_desc(auto sub () {
        return action_names(DataProviderActionCatalog::getActionHash(PendingApp)); });
    l += \"pending runs after: \" + getCalls(\"pending\");
    l += \"scheme runs before: \" + getCalls(\"scheme\");
    l += \"scheme: \" + attempt_desc(auto sub () { return ConnectionSchemeCache::getScheme(Scheme).display_name; });
    l += \"scheme runs after: \" + getCalls(\"scheme\");
    return l;
}",
    },

    # a lazy registration that is not deferred module initialization fails for a restricted reader, and is run again
    # by the next reader
    "late": {
        "trusted_pre": "%requires DataProvider
%requires ConnectionProvider
%requires RcLazyMod
list<string> sub run(*string arg) {
    registerLate();
    return (\"late registered\",);
}",
        "restricted": "%requires DataProvider
%requires ConnectionProvider
%requires RcLazyMod
list<string> sub run(*string arg) {
    list<string> l = ();
    l += \"restricted read: \" + attempt(auto sub () { return action_names(DataProviderActionCatalog::getActionHash(LateApp)); });
    l += \"restricted read again: \" + attempt(auto sub () {
        return action_names(DataProviderActionCatalog::getActionHash(LateApp)); });
    l += \"late runs: \" + getCalls(\"late\");
    l += \"failures: \" + attempt(auto sub () {
        return DataProviderActionCatalog::getInitializationFailures((LateApp,)).size(); });
    return l;
}",
        "trusted_post": "%requires DataProvider
%requires ConnectionProvider
%requires RcLazyMod
list<string> sub run(*string arg) {
    list<string> l = ();
    l += \"unrestricted read: \" + action_names(DataProviderActionCatalog::getActionHash(LateApp));
    l += \"late runs: \" + getCalls(\"late\");
    return l;
}",
        # the restricted Program reads the registration that the unrestricted reader ran
        "restricted_post": "list<string> sub run_post(*string arg) {
    return (\"restricted read after: \" + attempt(auto sub () {
        return action_names(DataProviderActionCatalog::getActionHash(LateApp)); }),);
}",
    },

    # restricted code cannot give its own code the exemption of deferred module initialization
    "closures": {
        "restricted": "%requires DataProvider
%requires ConnectionProvider
%requires RcLazyMod
int sub own_function() { return 1; }
list<string> sub run(*string arg) {
    list<string> l = ();
    code own = auto sub () { return 1; };
    l += \"own closure qualifies: \" + sprintf(\"%y\", is_module_init_closure(own));
    l += \"own closure run: \" + attempt(auto sub () { return call_module_init_closure(own); });
    l += \"own function reference qualifies: \" + sprintf(\"%y\", is_module_init_closure(\\own_function()));
    l += \"own function reference run: \" + attempt(auto sub () { return call_module_init_closure(\\own_function()); });
    code late = getLateClosure();
    l += \"late closure qualifies: \" + sprintf(\"%y\", is_module_init_closure(late));
    l += \"late closure run: \" + attempt(auto sub () { return call_module_init_closure(late); });
    l += \"late closure called: \" + attempt(late);
    code init = getInitClosure();
    l += \"init closure qualifies: \" + sprintf(\"%y\", is_module_init_closure(init));
    l += \"init closure run: \" + attempt(auto sub () { return call_module_init_closure(init); });
    l += \"init closure called: \" + attempt(init);
    l += \"module function reference qualifies: \" + sprintf(\"%y\", is_module_init_closure(\\getCalls()));
    l += \"register lazy actions: \" + attempt(auto sub () {
        return call_static_method(\"DataProvider::DataProviderActionCatalog\", \"registerLazyAppActions\", LazyApp,
            sub () {});
    });
    l += \"register pending app: \" + attempt(auto sub () {
        return call_static_method(\"DataProvider::DataProviderActionCatalog\", \"registerPendingApp\",
            \"QoreTestRcOwnPendingApp\", init);
    });
    l += \"lazy runs: \" + getCalls(\"lazy\");
    return l;
}",
        # a direct call is refused when it is parsed
        "restricted_parse": "%requires DataProvider
%requires ConnectionProvider
%requires RcLazyMod
sub t() { DataProviderActionCatalog::registerLazyAppActions(LazyApp, sub () {}); }",
    },
};

# prints the lines that the code of a case returns
sub print_lines(softlist<string> l) {
    map printf("%s\n", $1), l;
}

string case_name = ARGV[0];
string mode = ARGV[1] ?? "ast";
bool sandbox = ARGV[2] == "1";
*string arg = ARGV[3];

hash<auto> c = Cases{case_name};
if (!c) {
    printf("unknown case %y\n", case_name);
    exit(1);
}

*Program rp;
SandboxManager mgr();
if (c.restricted) {
    rp = new Program(PO_NEW_STYLE | Restrictions);
    if (sandbox) {
        rp.setSandboxManager(mgr);
    }
    rp.setExecMode(Modes{mode});
    try {
        rp.parse(Common + c.restricted + (c.restricted_post ?? ""), "restricted");
    } catch (hash<ExceptionInfo> ex) {
        printf("parse: %s: %s\n", ex.err, ex.desc);
        exit(1);
    }
}

# the unrestricted Programs are kept until the end, as what they register (ex: a call reference) refers to them
*Program tpre;
if (c.trusted_pre) {
    tpre = new Program(PO_NEW_STYLE);
    tpre.parse(Common + c.trusted_pre, "trusted-pre");
    print_lines(tpre.callFunction("run", arg));
}

if (rp) {
    print_lines(rp.callFunction("run", arg));
}

*Program tpost;
if (c.trusted_post) {
    tpost = new Program(PO_NEW_STYLE);
    tpost.parse(Common + c.trusted_post, "trusted-post");
    print_lines(tpost.callFunction("run", arg));
}

if (c.restricted_post) {
    print_lines(rp.callFunction("run_post", arg));
}

if (c.restricted_parse) {
    Program pp(PO_NEW_STYLE | Restrictions);
    if (sandbox) {
        pp.setSandboxManager(mgr);
    }
    pp.setExecMode(Modes{mode});
    try {
        pp.parse(c.restricted_parse, "restricted-parse");
        printf("direct call: parsed\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("direct call: %s: %s\n", ex.err,
            (ex.desc =~ /functional domain: [A-Z_]+/) ? (ex.desc =~ x/(functional domain: [A-Z_]+)/)[0] : ex.desc);
    }
}
