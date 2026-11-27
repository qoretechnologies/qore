#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a worker of foreign-program-code.qtest: the trust tests of the calling code count code compiled in another Program
# than the one it runs in as the code of the Program that compiled it (qore#7202); each case gives the outcome of the
# tenant's own code and of the same code running in another Program, which must both be the expected one; prints one
# "<case>=ok" line per case, or "<case>=FAIL: <details>"

%modern
%no-child-restrictions
%prepend-module-path "${SCRIPT_DIR}"
%requires ForeignCodeCheck

#! the restrictions of the restricted tenant
const TenantRestrictions = PO_NO_FILESYSTEM | PO_NO_NETWORK | PO_NO_EXTERNAL_INFO | PO_NO_TERMINAL_IO;

#! each probe runs in the tenant's own code and in code of the tenant that runs in another Program
const TenantCode = "%modern
%requires ForeignCodeCheck
string sub outcome(code c) {
    try {
        return sprintf(\"%y\", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! runs the probe in the tenant's own code
auto sub run_probe(string name, auto arg) {
    switch (name) {
        case \"lift\": { SandboxManager sm = arg; sm.setFilesystemDefaultPolicy(True); return True; }
        case \"isolation\": { SandboxManager sm = arg; sm.setIsolationDomain(\"tenant-x\"); return True; }
        case \"registry\": { return get_app_registry(\"qore-7202-host-value\"); }
        case \"set-manager\": { Program p = arg[0]; p.setSandboxManager(arg[1]); return True; }
        case \"debugger\": { object o = create_object(\"Breakpoint\"); return o.className(); }
        case \"governed\": { return SandboxManager::isGoverned(); }
        case \"restricted\": { return SandboxManager::isCallerRestricted(DOM_FILESYSTEM); }
        case \"sandboxed\": { return SandboxManager::isSandboxed(); }
        case \"module-registry\": { return ForeignCodeCheck::registry(\"qore-7202-host-value\"); }
        case \"module-lift\": { return ForeignCodeCheck::lift_own(); }
        case \"module-barrier\": { return ForeignCodeCheck::barrier(); }
        case \"module-cmd\": { Program p = arg; p.issueModuleCmd(\"qore-7202-no-such-module\", \"x\"); return True; }
        case \"defer\": { return defer_thread_cancel(string sub () {
                arg.ready.dec();
                try {
                    arg.go.waitForZero();
                    Mutex m();
                    m.lock();
                    m.unlock();
                    return \"completed\";
                } catch (hash<ExceptionInfo> ex) {
                    return ex.err == \"THREAD-CANCELLED\" ? \"cancelled\" : ex.err;
                }
            }); }
    }
    throw \"UNKNOWN\", name;
}

#! the outcome of the probe in the tenant's own code
string sub own(string name, auto arg) {
    return outcome(auto sub () { return run_probe(name, arg); });
}

public class TenantClass {
    #! the probe in code of the tenant that runs in another Program
    code foreign(string name, auto arg) {
        return string sub () {
            try {
                return sprintf(\"%y\", (auto sub () {
                switch (name) {
                    case \"lift\": { SandboxManager sm = arg; sm.setFilesystemDefaultPolicy(True); return True; }
                    case \"isolation\": { SandboxManager sm = arg; sm.setIsolationDomain(\"tenant-x\"); return True; }
                    case \"registry\": { return get_app_registry(\"qore-7202-host-value\"); }
                    case \"set-manager\": { Program p = arg[0]; p.setSandboxManager(arg[1]); return True; }
                    case \"debugger\": { object o = create_object(\"Breakpoint\"); return o.className(); }
                    case \"governed\": { return SandboxManager::isGoverned(); }
                    case \"restricted\": { return SandboxManager::isCallerRestricted(DOM_FILESYSTEM); }
                    case \"sandboxed\": { return SandboxManager::isSandboxed(); }
                    case \"module-registry\": { return ForeignCodeCheck::registry(\"qore-7202-host-value\"); }
                    case \"module-lift\": { return ForeignCodeCheck::lift_own(); }
                    case \"module-barrier\": { return ForeignCodeCheck::barrier(); }
                    case \"module-cmd\": { Program p = arg; p.issueModuleCmd(\"qore-7202-no-such-module\", \"x\"); return True; }
                    case \"defer\": { return defer_thread_cancel(string sub () {
                            arg.ready.dec();
                            try {
                                arg.go.waitForZero();
                                Mutex m();
                                m.lock();
                                m.unlock();
                                return \"completed\";
                            } catch (hash<ExceptionInfo> ex) {
                                return ex.err == \"THREAD-CANCELLED\" ? \"cancelled\" : ex.err;
                            }
                        }); }
                }
                throw \"UNKNOWN\", name;
                })());
            } catch (hash<ExceptionInfo> ex) {
                return ex.err;
            }
        };
    }

    #! the probe in code of the tenant that runs in another Program, giving its value
    code callback(string name, auto arg) {
        return auto sub () {
            switch (name) {
                case \"lift\": { SandboxManager sm = arg; sm.setFilesystemDefaultPolicy(True); return True; }
                case \"isolation\": { SandboxManager sm = arg; sm.setIsolationDomain(\"tenant-x\"); return True; }
                case \"registry\": { return get_app_registry(\"qore-7202-host-value\"); }
                case \"set-manager\": { Program p = arg[0]; p.setSandboxManager(arg[1]); return True; }
                case \"debugger\": { object o = create_object(\"Breakpoint\"); return o.className(); }
                case \"governed\": { return SandboxManager::isGoverned(); }
                case \"restricted\": { return SandboxManager::isCallerRestricted(DOM_FILESYSTEM); }
                case \"sandboxed\": { return SandboxManager::isSandboxed(); }
                case \"module-registry\": { return ForeignCodeCheck::registry(\"qore-7202-host-value\"); }
                case \"module-lift\": { return ForeignCodeCheck::lift_own(); }
                case \"module-barrier\": { return ForeignCodeCheck::barrier(); }
                case \"module-cmd\": { Program p = arg; p.issueModuleCmd(\"qore-7202-no-such-module\", \"x\"); return True; }
                case \"defer\": { return defer_thread_cancel(string sub () {
                        arg.ready.dec();
                        try {
                            arg.go.waitForZero();
                            Mutex m();
                            m.lock();
                            m.unlock();
                            return \"completed\";
                        } catch (hash<ExceptionInfo> ex) {
                            return ex.err == \"THREAD-CANCELLED\" ? \"cancelled\" : ex.err;
                        }
                    }); }
            }
            throw \"UNKNOWN\", name;
        };
    }
}

sub export_to(Program target) {
    target.importClass(\"TenantClass\");
}
";

int errors = 0;

sub report(string name, auto expected, auto actual) {
    if (expected == actual) {
        printf("%s=ok\n", name);
    } else {
        printf("%s=FAIL: expected %y got %y\n", name, expected, actual);
        ++errors;
    }
}

#! a tenant and an object whose code of the tenant runs in a Program of the host
class Tenant {
    public {
        Program tenant;
        Program host_pgm;
        object obj;
    }

    constructor(int po, *SandboxManager sm, int host_po = PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS) {
        tenant = new Program(po);
        if (sm) {
            tenant.setSandboxManager(sm);
        }
        tenant.parse(TenantCode, "tenant");
        host_pgm = new Program(host_po);
        tenant.callFunction("export_to", host_pgm);
        host_pgm.parse("object sub make() { return new TenantClass(); }", "host-program");
        obj = host_pgm.callFunction("make");
    }

    #! checks that the tenant's own code and its code that runs in the host's Program give the expected outcome
    check(string label, string name, string expected, auto arg, *auto foreign_arg) {
        report(label + "-own", expected, tenant.callFunction("own", name, arg));
        code c = obj.foreign(name, exists foreign_arg ? foreign_arg : arg);
        report(label + "-foreign", expected, c());
    }
}

set_app_registry("qore-7202-host-value", "host value");

# a restricted tenant may not change a sandbox that it holds or give it an isolation domain
{
    Tenant t(PO_NEW_STYLE | TenantRestrictions);
    SandboxManager m1();
    m1.setFilesystemDefaultPolicy(False);
    SandboxManager m2();
    m2.setFilesystemDefaultPolicy(False);
    t.check("restricted-lift", "lift", "SANDBOX-ACCESS-ERROR", m1, m2);
    report("restricted-lift-policy", False, m2.getFilesystemDefaultPolicy());
    t.check("restricted-isolation", "isolation", "SANDBOX-ACCESS-ERROR", new SandboxManager(),
        new SandboxManager());
}

# a sandboxed tenant does not get the host's registry values, may not replace another Program's sandbox, and calls the
# sandbox introspection APIs as sandboxed code
{
    SandboxManager sm();
    sm.setUncontrolledApiPolicy(True);
    Tenant t(PO_NEW_STYLE, sm);
    t.check("sandboxed-registry", "registry", "null", NOTHING);
    Program v1(PO_NEW_STYLE);
    v1.setSandboxManager(new SandboxManager());
    Program v2(PO_NEW_STYLE);
    SandboxManager v2sm();
    v2.setSandboxManager(v2sm);
    t.check("sandboxed-set-manager", "set-manager", "SANDBOX-ACCESS-ERROR", (v1, new SandboxManager()),
        (v2, new SandboxManager()));
}
{
    # the sandbox refuses uncontrolled APIs: the introspection APIs are refused to sandboxed code
    SandboxManager sm();
    Tenant t(PO_NEW_STYLE, sm);
    t.check("sandboxed-introspection", "governed", "INVALID-FUNCTION-ACCESS", NOTHING);
}

# the explicitly-allowed options are those that both Programs have
{
    Tenant t(PO_NEW_STYLE, NOTHING, PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_ALLOW_DEBUGGER);
    t.check("debugger", "debugger", "CREATE-OBJECT-ERROR", NOTHING);
}

# module code that code of a tenant calls answers for itself in the trust tests, wherever that code runs
foreach hash<auto> tenant in ({"label": "sandboxed", "po": PO_NEW_STYLE, "sm": True},
        {"label": "restricted", "po": PO_NEW_STYLE | TenantRestrictions, "sm": False}) {
    # (a sandbox that refuses uncontrolled APIs, the default)
    *SandboxManager sm = tenant.sm ? new SandboxManager() : NOTHING;
    Tenant t(tenant.po, sm);
    t.check(tenant.label + "-module-registry", "module-registry", "\"host value\"", NOTHING);
    t.check(tenant.label + "-module-lift", "module-lift", "\"lifted\"", NOTHING);
    t.check(tenant.label + "-module-barrier", "module-barrier", "\"barrier\"", NOTHING);
}

# a sandboxed tenant may not issue module commands to another Program
{
    SandboxManager sm();
    Tenant t(PO_NEW_STYLE, sm);
    t.check("sandboxed-module-cmd", "module-cmd", "RUNTIME-COMMAND-ERROR", new Program(PO_NEW_STYLE),
        new Program(PO_NEW_STYLE));
}

#! runs code of a sandboxed tenant that defers cancellation in a thread that is cancelled meanwhile; returns
#! "cancelled" if the code was cancelled, "completed" if it ran with cancellation deferred
string sub run_deferred(code c, hash<auto> arg) {
    Queue q();
    int tid = background sub () {
        try {
            q.push(c());
        } catch (hash<ExceptionInfo> ex) {
            q.push(ex.err);
        }
    }();
    arg.ready.waitForZero();
    cancel_thread(tid, "qore#7202 test");
    arg.go.dec();
    string rv = q.get();
    thread_join(tid);
    return rv;
}

# sandboxed code never runs with cancellation deferred
{
    SandboxManager sm();
    Tenant t(PO_NEW_STYLE, sm);
    hash<auto> arg = {"ready": new Counter(1), "go": new Counter(1)};
    report("sandboxed-defer-own", "cancelled", run_deferred(auto sub () {
        return t.tenant.callFunction("run_probe", "defer", arg);
    }, arg));
    arg = {"ready": new Counter(1), "go": new Counter(1)};
    report("sandboxed-defer-foreign", "cancelled", run_deferred(t.obj.callback("defer", arg), arg));
}

#! runs the code in the destructor of an object of the host
class HostHolder {
    private:internal {
        code c;
        Queue q;
    }

    constructor(code c, Queue q) {
        self.c = c;
        self.q = q;
    }

    destructor() {
        q.push(c());
    }
}

# the host's callbacks that run the tenant's code run it as the tenant's code
{
    Tenant t(PO_NEW_STYLE | TenantRestrictions);
    Queue q();
    code c = t.obj.callback("restricted", NOTHING);
    background q.push(c());
    report("callback-background", True, q.get());
    ThreadPool tp(1);
    tp.submit(sub () { q.push(c()); });
    report("callback-thread-pool", True, q.get());
    tp.stopWait();
    # a timer of the asynchronous I/O controller
    AsyncIoController::addTimerDelay(1ms, sub () { q.push(c()); });
    report("callback-timer", True, q.get());
    # the destructor of an object of the host
    {
        HostHolder h(c, q);
    }
    report("callback-destructor", True, q.get());
    SandboxManager sm();
    Tenant s(PO_NEW_STYLE, sm);
    c = s.obj.callback("sandboxed", NOTHING);
    tp = new ThreadPool(1);
    tp.submit(sub () { q.push(c()); });
    report("callback-thread-pool-sandboxed", True, q.get());
    tp.stopWait();
}

# a run without failures ends normally, so that its leak report under valgrind is meaningful
if (errors) {
    exit(1);
}
