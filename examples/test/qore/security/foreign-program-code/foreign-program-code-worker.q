#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the worker of foreign-program-code.qtest: code compiled in one Program that runs in another counts for the checks
# of the calling code with the restrictions and the sandbox of the Program that compiled it (qore#7202); prints one
# "<case>=ok" line per case, or "<case>=FAIL: <details>"

%modern
%no-child-restrictions
%prepend-module-path "${SCRIPT_DIR}"
%requires ForeignCodeCheck

#! the restrictions of the restricted tenant Programs
const TenantRestrictions = PO_NO_FILESYSTEM | PO_NO_NETWORK | PO_NO_EXTERNAL_INFO | PO_NO_TERMINAL_IO;

#! the code of a tenant, parts of which run in another Program
const TenantCode = "%modern
%requires ForeignCodeCheck
const TenantConst = \"tenant-const\";
hashdecl TenantHash { int a = 1; }
string sub helper() { return \"helper\"; }
public class TenantClass {
    #! the checks as the code itself sees them
    code direct() {
        return hash<auto> sub () {
            return {
                \"fs\": SandboxManager::isCallerRestricted(DOM_FILESYSTEM),
                \"net\": SandboxManager::isLentToRestricted(DOM_NETWORK),
                \"ext\": SandboxManager::callerMayReadExternalInfo(),
                \"secrets\": SandboxManager::callerMayReceiveHostSecrets(),
                \"term\": SandboxManager::isTerminalIoAllowed(),
                \"sandboxed\": SandboxManager::isSandboxed(),
                \"po_fs\": boolean(get_parse_options() & PO_NO_FILESYSTEM),
            };
        };
    }

    #! deletes the tenant's Program with host code given, then uses the tenant's code and constants
    code selfDelete(code del) {
        return hash<auto> sub () {
            del();
            hash<auto> r;
            try {
                r.helper = helper();
            } catch (hash<ExceptionInfo> ex) {
                r.helper = ex.err;
            }
            r.c = TenantConst;
            return r;
        };
    }

    #! waits until released, then uses the tenant's code and constants
    code waitThenUse(Queue started, Queue go) {
        return hash<auto> sub () {
            started.push(1);
            go.get();
            hash<auto> r;
            try {
                r.helper = helper();
            } catch (hash<ExceptionInfo> ex) {
                r.helper = ex.err;
            }
            r.c = TenantConst;
            return r;
        };
    }

    #! waits until released if the value received is True, then uses the tenant's code, constants and types;
    #! returns \"warm\" for False, so that it can be called often first
    code hotWait(Queue started, Queue go) {
        return auto sub () {
            started.push(1);
            if (!go.get()) {
                return \"warm\";
            }
            try {
                throw \"TENANT-ERROR\";
            } catch (hash<ExceptionInfo> ex) {
                return {\"err\": ex.err, \"c\": TenantConst, \"a\": cast<hash<TenantHash>>({\"a\": 2}).a};
            }
        };
    }

    #! starts a thread whose expression gives its TID, waits until released, then uses the tenant's constants and
    #! types
    code bgUse(Queue started, Queue go, Queue res) {
        return sub () {
            background res.push((started.push(gettid()), go.get(),
                {\"a\": cast<hash<TenantHash>>({\"a\": 2}).a, \"c\": TenantConst})[2]);
        };
    }

    #! the checks as module code that the code calls sees them
    code viaModule() {
        return hash<auto> sub () { return ForeignCodeCheck::checks(); };
    }

    #! calls the code that it is given (host code), through module code
    code callBack(code c) {
        return auto sub () { return ForeignCodeCheck::call(c); };
    }

    #! calls the code that it is given (host code) directly
    code callDirect(code c) {
        return auto sub () { return c(); };
    }

    #! the check of the code of a thread that the code starts
    code inThread() {
        return bool sub () {
            Queue q();
            background q.push(SandboxManager::isCallerRestricted(DOM_FILESYSTEM));
            return q.get();
        };
    }

    #! calls a function by name
    code byName(string name) {
        return auto sub () { return call_function(name, \"HOME\"); };
    }
}

sub export_to(Program target) {
    target.importClass(\"TenantClass\");
}
";

#! the code of a sandboxed tenant whose parse options do not restrict the file system or the network
const PolicyTenantCode = "%modern
public class TenantClass {
    #! reads a file
    code readFile(string path) {
        return string sub () { return ReadOnlyFile::readTextFile(path); };
    }

    #! reads a file in code that the code creates
    code nested(string path) {
        return string sub () {
            code inner = string sub () { return ReadOnlyFile::readTextFile(path); };
            return inner();
        };
    }

    #! returns the status of a file with a builtin function
    code statFile(string path) {
        return list<auto> sub () { return stat(path); };
    }

    #! connects to a socket
    code connect(string target) {
        return string sub () {
            Socket s();
            try {
                s.connect(target, 2s);
            } catch (hash<ExceptionInfo> ex) {
                return ex.err;
            }
            return \"connected\";
        };
    }

}

sub export_to(Program target) {
    target.importClass(\"TenantClass\");
}
";

#! the checks of a trusted caller
const Trusted = {
    "fs": False, "net": False, "ext": True, "secrets": True, "term": True, "sandboxed": False,
};

#! the checks of a caller restricted with TenantRestrictions and not sandboxed
const Restricted = {
    "fs": True, "net": True, "ext": False, "secrets": False, "term": False, "sandboxed": False,
};

int errors = 0;

#! calls the code given: one call site, which the native code of the IR and JIT modes makes directly once the code
#! has been called often
auto sub call_closure(code c) {
    return c();
}

sub report(string name, auto expected, auto actual) {
    if (expected == actual) {
        printf("%s=ok\n", name);
    } else {
        printf("%s=FAIL: expected %y got %y\n", name, expected, actual);
        ++errors;
    }
}

#! returns the hash without the keys that are not compared
hash<auto> sub checks(hash<auto> h) {
    return h - "po_fs";
}

#! returns a tenant Program and an object whose code of the tenant runs in a Program of the host
/** The tenant has the given restrictions and sandbox; the host Program is a Program of the host without restrictions
    that the tenant's code was given
*/
object sub make_object(int po, *SandboxManager sm, reference<Program> tenant, reference<Program> host_pgm,
        string code = TenantCode) {
    tenant = new Program(po);
    if (sm) {
        tenant.setSandboxManager(sm);
    }
    tenant.parse(code, "tenant");
    host_pgm = new Program(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
    tenant.callFunction("export_to", host_pgm);
    host_pgm.parse("object sub make() { return new TenantClass(); }", "host-program");
    return host_pgm.callFunction("make");
}

# a restricted tenant: its code is the tenant's code where it runs
{
    Program tenant;
    Program host_pgm;
    object o = make_object(TenantRestrictions, NOTHING, \tenant, \host_pgm);
    code c = o.direct();
    hash<auto> h = c();
    report("restricted-direct", Restricted, checks(h));
    report("restricted-runtime-options", True, h.po_fs);
    c = o.viaModule();
    report("restricted-module", Restricted, c());
    # host code that the tenant's code calls answers for itself, directly and through module code
    code host_check = hash<auto> sub () { return ForeignCodeCheck::checks(); };
    c = o.callDirect(host_check);
    report("restricted-calls-host", Trusted, c());
    c = o.callBack(host_check);
    report("restricted-module-calls-host", Trusted, c());
    # a function found by name is checked against the tenant's restrictions
    c = o.byName("getenv");
    try {
        c();
        report("restricted-by-name", "exception", "no exception");
    } catch (hash<ExceptionInfo> ex) {
        report("restricted-by-name", "exception", "exception");
    }
    # a thread that the tenant's code starts runs the tenant's code
    c = o.inThread();
    report("restricted-thread", True, c());
    # a host grant (a system-policy capability) lends the external information domain to the code inside it
    code cap = SandboxManager::createSystemPolicyCall(bool sub () {
        return ForeignCodeCheck::checks().ext;
    });
    c = o.callDirect(cap);
    report("restricted-grant", True, c());
}

# a sandboxed tenant without restrictions: its code is sandboxed code where it runs
{
    SandboxManager sm();
    Program tenant;
    Program host_pgm;
    object o = make_object(PO_NEW_STYLE, sm, \tenant, \host_pgm);
    code c = o.direct();
    hash<auto> h = checks(c());
    report("sandboxed-direct-secrets", False, h.secrets);
    report("sandboxed-direct-sandboxed", True, h.sandboxed);
    c = o.viaModule();
    h = c();
    report("sandboxed-module-secrets", False, h.secrets);
}

# a sandboxed tenant whose policy refuses a file and a network target that its parse options do not restrict: its code
# is governed by its sandbox where it runs
{
    string dir = sprintf("%s%sqore-foreign-program-code-%d-%d", ENV.TMPDIR ?? "/tmp", DirSep, getpid(),
        now_us());
    mkdir(dir, 0700);
    on_exit {
        unlink(dir + DirSep + "file.txt");
        rmdir(dir);
    }
    string path = dir + DirSep + "file.txt";
    File f();
    f.open2(path, O_CREAT | O_WRONLY | O_TRUNC);
    f.write("data");
    f.close();
    Socket server();
    server.bind("127.0.0.1:0");
    server.listen();
    string target = sprintf("127.0.0.1:%d", server.getSocketInfo().port);

    SandboxManager sm();
    sm.addFilesystemDeniedPath(dir);
    sm.setNetworkDefaultPolicy(False);
    Program tenant;
    Program host_pgm;
    object o = make_object(PO_NEW_STYLE, sm, \tenant, \host_pgm, PolicyTenantCode);
    code c = o.readFile(path);
    try {
        c();
        report("sandboxed-policy-file", "refused", "read");
    } catch (hash<ExceptionInfo> ex) {
        report("sandboxed-policy-file", "refused", "refused");
    }
    c = o.connect(target);
    auto r = c();
    report("sandboxed-policy-network", True, r != "connected");
    # code that the tenant's code creates where it runs is governed as well
    c = o.nested(path);
    try {
        c();
        report("sandboxed-policy-nested", "refused", "read");
    } catch (hash<ExceptionInfo> ex) {
        report("sandboxed-policy-nested", "refused", "refused");
    }
    # a sandbox that the tenant's Program gets after its code was given out governs that code as well
    {
        SandboxManager later();
        later.addFilesystemDeniedPath(dir);
        Program tenant2;
        Program host_pgm2;
        object o2 = make_object(PO_NEW_STYLE, NOTHING, \tenant2, \host_pgm2, PolicyTenantCode);
        code c2 = o2.readFile(path);
        code c3 = o2.statFile(path);
        tenant2.setSandboxManager(later);
        try {
            c2();
            report("sandboxed-policy-later", "refused", "read");
        } catch (hash<ExceptionInfo> ex) {
            report("sandboxed-policy-later", "refused", "refused");
        }
        try {
            c3();
            report("sandboxed-policy-later-function", "refused", "allowed");
        } catch (hash<ExceptionInfo> ex) {
            report("sandboxed-policy-later-function", "refused", ex.err == "FILESYSTEM-ACCESS-DENIED" ? "refused" : ex.err);
        }
        # once the tenant's Program is deleted, its code no longer runs
        delete tenant2;
        try {
            c3();
            report("deleted-tenant", "PROGRAM-ERROR", "ran");
        } catch (hash<ExceptionInfo> ex) {
            report("deleted-tenant", "PROGRAM-ERROR", ex.err);
        }
    }
    # the host's own code is not governed by the tenant's sandbox
    report("sandboxed-policy-host-file", "data", ReadOnlyFile::readTextFile(path));
    server.close();
}

# the deletion of a tenant's Program waits for its code that runs in another Program
{
    hash<auto> used = {"helper": "helper", "c": "tenant-const"};
    SandboxManager sm();
    Program tenant;
    Program host_pgm;
    object o = make_object(PO_NEW_STYLE, sm, \tenant, \host_pgm);
    # a deletion by the code itself takes effect once the code has left
    code c = o.selfDelete(sub () { delete tenant; });
    report("deleted-self", used, c());
    try {
        c();
        report("deleted-self-after", "PROGRAM-ERROR", "ran");
    } catch (hash<ExceptionInfo> ex) {
        report("deleted-self-after", "PROGRAM-ERROR", ex.err);
    }
    # a deletion in another thread waits for the code (the teardown hold needs a library with debugging)
    if (Option::HAVE_LIBRARY_DEBUGGING) {
        Program tenant2;
        Program host_pgm2;
        object o2 = make_object(PO_NEW_STYLE, sm, \tenant2, \host_pgm2);
        Queue started();
        Queue go();
        code c2 = o2.waitThenUse(started, go);
        call_function("dbg_hold_program_teardown", tenant2);
        Counter deleted(1);
        background sub () {
            delete tenant2;
            deleted.dec();
        }();
        bool held = call_function("dbg_wait_program_teardown_held", 30000);
        Queue result();
        background sub () {
            try {
                result.push(c2());
            } catch (hash<ExceptionInfo> ex) {
                result.push(ex.err);
            }
        }();
        started.get();
        # the teardown marks the Program and waits for the thread that runs its code
        bool waiting = call_function("dbg_continue_program_teardown", 30000);
        int pending = deleted.getCount();
        go.push(1);
        auto r = result.get();
        deleted.waitForZero();
        report("deleted-wait", {"held": True, "waiting": True, "pending": 1, "result": used},
            {"held": held, "waiting": waiting, "pending": pending, "result": r});

    }
    # the thread that runs such code is counted in the tenant's Program, whose deletion waits for its threads: also
    # for code that the native code of the IR and JIT modes calls directly once it is called often, and for the
    # expression of a thread that such code starts
    {
        Program tenant3;
        Program host_pgm3;
        object o3 = make_object(PO_NEW_STYLE, sm, \tenant3, \host_pgm3);
        Queue started();
        Queue go();
        code c3 = o3.hotWait(started, go);
        for (int i = 0; i < 3000; ++i) {
            go.push(False);
            call_closure(c3);
            started.get();
        }
        Queue result();
        Queue tids();
        background sub () {
            tids.push(gettid());
            try {
                result.push(call_closure(c3));
            } catch (hash<ExceptionInfo> ex) {
                result.push(ex.err);
            }
        }();
        int tid = tids.get();
        started.get();
        bool counted = inlist(tid, tenant3.getThreadList());
        go.push(True);
        auto r = result.get();
        report("held-hot", {"counted": True, "result": {"err": "TENANT-ERROR", "c": "tenant-const", "a": 2}},
            {"counted": counted, "result": r});
        report("held-hot-released", False, inlist(tid, tenant3.getThreadList()));

        Program tenant4;
        Program host_pgm4;
        object o4 = make_object(PO_NEW_STYLE, sm, \tenant4, \host_pgm4);
        started = new Queue();
        go = new Queue();
        result = new Queue();
        code c4 = o4.bgUse(started, go, result);
        c4();
        tid = started.get();
        counted = inlist(tid, tenant4.getThreadList());
        go.push(1);
        r = result.get();
        report("held-background", {"counted": True, "result": {"a": 2, "c": "tenant-const"}},
            {"counted": counted, "result": r});
    }

}

# a tenant without restrictions: its code is not affected
{
    Program tenant;
    Program host_pgm;
    object o = make_object(PO_NEW_STYLE, NOTHING, \tenant, \host_pgm);
    code c = o.direct();
    report("unrestricted-direct", Trusted, checks(c()));
    c = o.viaModule();
    report("unrestricted-module", Trusted, c());
}

# a run without failures ends normally, so that its leak report under valgrind is meaningful
if (errors) {
    exit(1);
}
