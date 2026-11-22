#!/usr/bin/env qore
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the script of reflection-module-init-barrier-oom.qtest: the barrier of a library module's initialization that cannot
# be recorded for lack of memory (debug hook); the case is given as the argument, one result is printed per line

%modern
%requires reflection
%requires ThBarrierOomFixture

# the functions of the sandboxed Program
const TenantSource = "%modern
%requires reflection
%requires ThBarrierOomFixture
hashdecl DefDecl { string m = 'd'; }
Reflection::TypedHashMember sub member() {
    return (new Reflection::TypedHash('DefDecl')).getMember('m');
}
# a closure of this Program that has library module code read the default value of its own typed hash
code sub module_default_closure() {
    Reflection::TypedHashMember mem = member();
    return string sub () { return ThBarrierOomFixture::default_of(mem); };
}
# the barrier is set for the load of library code by this Program (its sandbox must allow uncontrolled APIs)
hash<auto> sub barrier_call(code c) {
    return call_function('dbg_module_init_barrier_call', c);
}
sub arm(int count) {
    call_function('dbg_fail_next_module_init_barrier_records', count);
}
sub load(string name) {
    load_module(name);
}
";

#! a sandboxed Program; with \a uncontrolled, its sandbox allows uncontrolled APIs (it can call debug builtins)
Program sub tenant(bool uncontrolled = False) {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_NO_CHILD_PO_RESTRICTIONS
        | PO_NO_INHERIT_USER_HASHDECLS);
    SandboxManager sm();
    if (uncontrolled) {
        sm.setUncontrolledApiPolicy(True);
    }
    p.setSandboxManager(sm);
    p.parse(TenantSource, "tenant");
    return p;
}

sub out(string line) {
    stdout.print(line + "\n");
    stdout.sync();
}

string sub unrecorded() {
    return sprintf("unrecorded: %d", call_function("dbg_module_init_barrier_unrecorded"));
}

string sub barrier_result(hash<auto> h) {
    return sprintf("failed: %y, unrecorded during: %d, result: %s", h.failed, h.unrecorded, h.result);
}

switch (ARGV[0]) {
    # library code loaded by trusted code: the load fails before any of the module's code runs
    case "load-trusted": {
        out("before load");
        call_function("dbg_fail_next_module_init_barrier_records", 1);
        load_module("ThBarrierOomLoad");
        out("after load");
        break;
    }

    # library code loaded by a sandboxed Program: the same, and the references to its sandbox are released
    case "load-sandbox": {
        Program a = tenant();
        out("before load");
        # the hook is armed for the current thread, where the sandboxed Program's code runs
        call_function("dbg_fail_next_module_init_barrier_records", 1);
        a.callFunction("load", "ThBarrierOomLoad");
        out("after load");
        break;
    }

    # library module code that runs while the barrier is active finds no application Program
    case "reflect": {
        Program a = tenant();
        code c = a.callFunction("module_default_closure");
        out("before: " + c());
        out("recorded barrier: " + barrier_result(call_function("dbg_module_init_barrier_call", c)));
        call_function("dbg_fail_next_module_init_barrier_records", 1);
        out("unrecorded barrier: " + barrier_result(call_function("dbg_module_init_barrier_call", c)));
        out("after: " + unrecorded() + ", " + c());
        # the barrier set for a sandboxed Program, whose sandbox the helper references
        Program b = tenant(True);
        out("sandbox recorded barrier: " + barrier_result(b.callFunction("barrier_call", c)));
        b.callFunction("arm", 1);
        out("sandbox unrecorded barrier: " + barrier_result(b.callFunction("barrier_call", c)));
        out("sandbox after: " + unrecorded() + ", " + c());
        # a normal load works afterwards
        load_module("ThBarrierOomLoad");
        out(sprintf("load after: %s, init ran: %y", unrecorded(), call_function("ThBarrierOomLoad::init_ran")));
        break;
    }

    default:
        throw "UNKNOWN-CASE", sprintf("%y", ARGV[0]);
}
