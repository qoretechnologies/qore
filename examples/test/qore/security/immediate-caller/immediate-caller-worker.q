#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The worker of immediate-caller.qtest: the immediate caller (SandboxManager::getImmediateCaller(), qore#7302) of
# library code that the code of other Programs and other modules calls; prints "<case>=ok" or what the case got
# instead.  The argument is the execution mode of the tenant Programs that it creates (default: ast)

%modern
%no-child-restrictions

%requires ImmAsk
%requires ImmOther
%requires ImmInit
%requires Logger
%requires ./ImmPath.qm

#! the code of each tenant
const TenantCode = "%modern
%requires ImmAsk
%requires ImmOther
%requires Logger
*hash<auto> sub t_direct() { return ImmAsk::ask(); }
*hash<auto> sub t_via_other() { return ImmOther::ask_via(); }
bool sub t_is_module(string m) { return ImmAsk::is_module(m); }
bool sub t_is_module_via(string m) { return ImmOther::is_module_via(m); }
code sub t_closure_ask() { return *hash<auto> sub () { return ImmAsk::ask(); }; }
code sub t_closure_via() { return *hash<auto> sub () { return ImmOther::ask_via(); }; }
code sub t_ref_ask() { return \\ImmAsk::ask(); }
code sub t_ref_via() { return \\ImmOther::ask_via(); }
code sub t_ref_is_module() { return \\ImmOther::is_module_via(); }
auto sub t_module_calls_closure() { return ImmOther::call(*hash<auto> sub () { return ImmAsk::ask(); }); }
auto sub t_module_calls_ref() { return ImmOther::call(\\ImmAsk::ask()); }
auto sub t_module_calls_ref_via() { return ImmAsk::call(\\ImmOther::ask_via()); }
object sub t_thing() { return new ImmOther::Thing(); }
sub t_listen() { ImmAsk::add_listener(*hash<auto> sub () { return ImmAsk::ask(); }); }
class Appender inherits LoggerAppenderWithLayout {
    public {
        list<auto> got = ();
    }
    constructor() : LoggerAppenderWithLayout(\"t\", new LoggerLayoutPattern(\"%m%n\")) {
        open();
    }
    processEventImpl(int type, auto params) {
        if (type == EVENT_LOG) {
            push got, ImmAsk::ask();
        }
    }
}
list<auto> sub t_appender() {
    Appender a();
    Logger l(\"t\", LoggerLevel::getLevelInfo());
    l.addAppender(a);
    ImmOther::log_with(l);
    return a.got;
}
";

#! the execution modes of the tenant Programs
const Modes = {"ast": EM_AST, "ir": EM_IR, "jit": EM_JIT, "tiered": EM_TIERED};

#! the results, in the order the cases ran
list<string> results = ();

sub check(string name, auto expected, auto actual) {
    push results, actual === expected
        ? sprintf("%s=ok", name)
        : sprintf("%s=expected %y, got %y", name, expected, actual);
}

#! checks an immediate caller: \a exp gives the keys checked (and "trusted" for the presence of \c trusted_frame)
sub check_caller(string name, hash<auto> exp, *hash<auto> got) {
    if (!got) {
        push results, sprintf("%s=expected a caller, got %y", name, got);
        return;
    }
    hash<auto> h;
    foreach string k in (keys exp) {
        if (k == "trusted") {
            h.trusted = exists got.trusted_frame;
        } else {
            h{k} = got{k};
        }
    }
    check(name, exp, h);
}

#! returns a tenant Program
Program sub tenant(string name, int po, bool sandboxed, string mode) {
    Program p(PO_NEW_STYLE | PO_NO_CHILD_PO_RESTRICTIONS | po, True);
    p.setExecMode(Modes{mode});
    p.parse(TenantCode, "tenant-" + name);
    if (sandboxed) {
        p.setSandboxManager(new SandboxManager());
    }
    return p;
}

string mode = ARGV[0] ?? "ast";
int main_id = SandboxManager::getCallerProgramId();

# trusted host code
*hash<auto> direct = ImmAsk::ask();
check_caller("host", {"program_id": main_id, "module": NOTHING, "library": False, "restricted": False,
    "trusted": True}, direct);
check("host-frames", True, direct.trusted_frame <= direct.frame);
# the module's own nested function and a method of its class are skipped
*hash<auto> nested = ImmAsk::ask_nested();
check("nested", direct, nested);
check("method", direct, (new ImmAsk::Asker()).ask());
# another library module: named, and its own frame
*hash<auto> other = ImmOther::ask_via();
check_caller("other-module", {"program_id": other.program_id, "module": "ImmOther", "library": True,
    "restricted": False, "trusted": True}, other);
check("other-module-program", True, other.program_id != main_id);
check("other-module-frame", direct.frame + 1, other.frame);
check("other-module-trusted-frame", direct.trusted_frame, other.trusted_frame);
check("is-module-other", True, ImmOther::is_module_via("ImmOther"));
check("is-module-other-name", False, ImmOther::is_module_via("ImmAsk"));
check("is-module-host", False, ImmAsk::is_module("ImmOther"));
# the asking module called back by another module that it called: the other module
check_caller("called-back", {"module": "ImmOther", "restricted": False, "trusted": True}, ImmOther::ask_back());
# a module loaded by its path is not library code (its name is not checked: a qcc executable links it in)
*hash<auto> path = ImmPath::ask_via();
check_caller("path-module", {"library": False, "restricted": False, "trusted": True}, path);
check("is-module-path", False, ImmPath::is_module_via("ImmPath"));
# the initialization of library code: its own code is the caller (the module has a name only once it is registered,
# which an AOT module is before it is initialized)
check_caller("init", {"library": True, "restricted": False, "trusted": True},
    ImmInit::get().caller);
# a thread that module code started: the caller is unknown
check("module-thread", NOTHING, ImmAsk::ask_in_background().caller);

foreach list<auto> t in (("restricted", PO_NO_NETWORK, False), ("sandboxed", 0, True)) {
    string tn = t[0];
    # objects of library classes that restricted code created taint the code that calls their methods only if their
    # creator is restricted from a domain that a host grant lends (the restricted tenant, not the sandboxed one)
    bool obj_taints = (tn == "restricted");
    Program p = tenant(tn, t[1], t[2], mode);
    int pid = p.getProgramId();
    # restricted code itself
    check_caller(tn + "-direct", {"program_id": pid, "module": NOTHING, "library": False, "restricted": True,
        "trusted": False}, p.callFunction("t_direct"));
    check(tn + "-is-module", False, p.callFunction("t_is_module", "ImmOther"));
    # library code that restricted code calls is that library's code
    *hash<auto> via = p.callFunction("t_via_other");
    check_caller(tn + "-via-other", {"module": "ImmOther", "library": True, "restricted": False, "trusted": True},
        via);
    check(tn + "-via-other-run", via.frame, via.trusted_frame);
    check(tn + "-is-module-via", True, p.callFunction("t_is_module_via", "ImmOther"));
    # a closure of restricted code that module code calls is restricted code
    check_caller(tn + "-closure-called-by-module", {"program_id": pid, "restricted": True, "trusted": False},
        ImmOther::call(p.callFunction("t_closure_ask")));
    check_caller(tn + "-closure-called-by-module-own", {"program_id": pid, "restricted": True, "trusted": False},
        p.callFunction("t_module_calls_closure"));
    check_caller(tn + "-closure-called-by-host", {"program_id": pid, "restricted": True, "trusted": False},
        call_function(p.callFunction("t_closure_ask")));
    # library code that such a closure calls is the library's, but its run of trusted code ends at the closure
    *hash<auto> cv = ImmAsk::call(p.callFunction("t_closure_via"));
    check_caller(tn + "-closure-via-other", {"module": "ImmOther", "restricted": False, "trusted": True}, cv);
    check(tn + "-closure-via-other-run", cv.frame, cv.trusted_frame);
    # a call reference of restricted code to library code runs from the code of the Program that created it, which is
    # the immediate caller of the library code, whoever calls the reference
    check_caller(tn + "-ref-called-by-module", {"program_id": pid, "module": NOTHING, "restricted": True,
        "trusted": False}, ImmOther::call(p.callFunction("t_ref_ask")));
    check_caller(tn + "-ref-called-by-module-own", {"program_id": pid, "restricted": True, "trusted": False},
        p.callFunction("t_module_calls_ref"));
    check_caller(tn + "-ref-called-by-host", {"program_id": pid, "restricted": True, "trusted": False},
        call_function(p.callFunction("t_ref_ask")));
    # library code that such a reference reaches is that library's code, as when restricted code calls it directly
    check(tn + "-ref-is-module", True, ImmAsk::call_with(p.callFunction("t_ref_is_module"), "ImmOther"));
    # a call reference of restricted code to other library code that calls the asking module: the other module's
    # code, called by the module that called the reference, but its run of trusted code ends there
    *hash<auto> rv = p.callFunction("t_module_calls_ref_via");
    check_caller(tn + "-ref-via-other", {"module": "ImmOther", "restricted": False, "trusted": True}, rv);
    check(tn + "-ref-via-other-run", rv.frame, rv.trusted_frame);
    # an object of a library class that restricted code created, whose method module code calls
    check_caller(tn + "-object-called-by-module", {"module": "ImmOther", "restricted": obj_taints,
        "trusted": !obj_taints}, ImmOther::call_method(p.callFunction("t_thing")));
    # the object's method called by the host: its code is the library's, and the run of trusted code ends at the host
    # code that called the object of the restricted code
    *hash<auto> oh = p.callFunction("t_thing").ask();
    check_caller(tn + "-object-called-by-host", {"module": "ImmOther", "restricted": False, "trusted": True}, oh);
    check(tn + "-object-called-by-host-run", obj_taints, oh.frame == oh.trusted_frame);
    # a listener of restricted code that module code calls back
    ImmAsk::clear_listeners();
    p.callFunction("t_listen");
    list<auto> fired = ImmAsk::fire();
    ImmAsk::clear_listeners();
    check(tn + "-listener-count", 1, fired.size());
    check_caller(tn + "-listener", {"program_id": pid, "restricted": True, "trusted": False}, fired[0]);
    # a logger appender of restricted code that library code (Logger) calls when other library code logs
    list<auto> logged = p.callFunction("t_appender");
    check(tn + "-appender-count", 1, logged.size());
    check_caller(tn + "-appender", {"program_id": pid, "restricted": True, "trusted": False}, logged[0]);
}

# an unrestricted tenant's code is trusted
Program u = tenant("unrestricted", 0, False, mode);
check_caller("unrestricted-direct", {"program_id": u.getProgramId(), "restricted": False, "trusted": True},
    u.callFunction("t_direct"));
check_caller("unrestricted-closure", {"program_id": u.getProgramId(), "restricted": False, "trusted": True},
    ImmOther::call(u.callFunction("t_closure_ask")));
check_caller("unrestricted-ref", {"program_id": u.getProgramId(), "restricted": False, "trusted": True},
    ImmOther::call(u.callFunction("t_ref_ask")));

print(results.join("\n") + "\n");
