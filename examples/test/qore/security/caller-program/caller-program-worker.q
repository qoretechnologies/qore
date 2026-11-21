#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The worker of caller-program.qtest: the caller Program (SandboxManager::getCallerProgramId(), qore#6852) of module
# code that serves the code of other Programs; prints "<case>=ok" or what the case got instead

%modern
%no-child-restrictions
%prepend-module-path "${SCRIPT_DIR}"

%requires CallerProbeA
# library code (a trusted module directory: caller-program/lib), loaded by the host (qore#7077)
%requires CallerInitTrusted

#! the code of each tenant
const TenantCode = "%requires CallerProbeA
*int sub t_direct() { return SandboxManager::getCallerProgramId(); }
*int sub t_probe() { return CallerProbeA::probe(); }
*int sub t_via_b() { return CallerProbeA::via_b(); }
*int sub t_method() { return (new CallerProbeA::Probe()).probe(); }
*int sub t_background() { return CallerProbeA::in_background(); }
*int sub t_pool() { return CallerProbeA::in_pool(); }
code sub t_closure() { return *int sub () { return CallerProbeA::probe(); }; }
auto sub t_call_code(code c) { return CallerProbeA::call_code(c); }
auto sub t_call_own() { return CallerProbeA::call_code(*int sub () { return CallerProbeA::probe(); }); }
hash<auto> sub t_both() { return CallerProbeA::both(); }
";

#! the results, in the order the cases ran
list<string> results = ();

sub check(string name, auto expected, auto actual) {
    push results, actual === expected
        ? sprintf("%s=ok", name)
        : sprintf("%s=expected %y, got %y", name, expected, actual);
}

#! returns a tenant Program, sandboxed if \a sandboxed is True
Program sub tenant(string name, bool sandboxed, *string extra) {
    Program p(PO_NEW_STYLE | PO_NO_CHILD_PO_RESTRICTIONS, True);
    p.parse(TenantCode + (extra ?? ""), "tenant-" + name);
    if (sandboxed) {
        p.setSandboxManager(new SandboxManager());
    }
    return p;
}

int main_id = SandboxManager::getCallerProgramId();
check("main-direct", True, main_id > 0);
check("main-module", main_id, CallerProbeA::probe());

Program a = tenant("a", True);
Program b = tenant("b", False, sprintf("%%requires %s/TenantMod.qm\n"
    "*int sub t_tenant_mod() { return TenantMod::call_probe(); }\n", get_script_dir()));
int a_id = a.getProgramId();
int b_id = b.getProgramId();

# the code of a tenant, also through library module code calling more module code, a method and threads
check("direct", a_id, a.callFunction("t_direct"));
check("module", a_id, a.callFunction("t_probe"));
check("module-via-module", a_id, a.callFunction("t_via_b"));
check("method", a_id, a.callFunction("t_method"));
check("background", a_id, a.callFunction("t_background"));
check("thread-pool", a_id, a.callFunction("t_pool"));

# a closure runs its own Program's code wherever it is called
code a_closure = a.callFunction("t_closure");
check("closure-of-a-called-by-b", a_id, b.callFunction("t_call_code", a_closure));
check("closure-of-b", b_id, b.callFunction("t_call_own"));
check("closure-of-a-called-by-main", a_id, CallerProbeA::call_code(a_closure));

# the caller Program does not change the sandbox checks: tenant A is still refused
hash<auto> h = a.callFunction("t_both");
check("sandboxed-both-id", a_id, h.id);
check("sandboxed-both-allowed", False, h.allowed);
h = b.callFunction("t_both");
check("unsandboxed-both-id", b_id, h.id);
check("unsandboxed-both-allowed", True, h.allowed);

# a module that a tenant loads by its path is a module's Program, which serves the tenant
check("tenant-module", b_id, b.callFunction("t_tenant_mod"));

# a trusted build hides the code that it serves: the caller is unknown in the build and in a Program it creates
code build = SandboxManager::createTrustedBuildCall(hash<auto> sub () {
    Program p(PO_NEW_STYLE | PO_NO_CHILD_PO_RESTRICTIONS, True);
    p.parse(TenantCode, "built");
    return {"pgm": p, "in_build": CallerProbeA::probe()};
});
hash<auto> built = build();
check("trusted-build", NOTHING, built.in_build);
check("trusted-build-program", NOTHING, built.pgm.callFunction("t_probe"));

# the initialization of library code (a trusted module directory) creates state that the whole process shares: its
# caller is unknown, also in a thread that it starts, whoever loads it first; a module that a tenant loads by its own
# path serves that tenant (qore#7077)
h = CallerInitTrusted::get();
check("init-library-host-ran", True, h.ran);
check("init-library-host", NOTHING, h.init);
check("init-library-host-thread", NOTHING, h.thread);
Program init_a = tenant("init-a", True, "%requires CallerInitTenant\n"
    "hash<auto> sub t_init_get() { return CallerInitTenant::get(); }\n"
    + sprintf("%%requires %s/CallerInitPath.qm\n", get_script_dir())
    + "hash<auto> sub t_path_get() { return CallerInitPath::get(); }\n");
h = init_a.callFunction("t_init_get");
check("init-library-tenant-ran", True, h.ran);
check("init-library-tenant", NOTHING, h.init);
check("init-library-tenant-thread", NOTHING, h.thread);
# another tenant that uses the library afterwards: its own calls are its own, and it gets nothing of the first
Program init_b = tenant("init-b", True, "%requires CallerInitTenant\n"
    "hash<auto> sub t_init_get() { return CallerInitTenant::get(); }\n");
h = init_b.callFunction("t_init_get");
check("init-library-other", NOTHING, h.init);
check("init-library-other-call", init_b.getProgramId(), h.call);
h = init_a.callFunction("t_path_get");
check("init-path-ran", True, h.ran);
check("init-path-tenant", init_a.getProgramId(), h.init);
check("init-path-tenant-thread", init_a.getProgramId(), h.thread);

# a Program that module code creates (with code that a tenant gave it) is not a module's, although it inherits
# PO_IN_MODULE: its code is its own, whoever calls it (qore#7077)
Program maker(PO_NEW_STYLE);
maker.parse(sprintf("%%requires %s/ProgramMaker.qm\n"
    "Program sub t_make_program(string src) { return ProgramMaker::make_program(src); }\n", get_script_dir()),
    "maker");
maker.setSandboxManager(new SandboxManager());
Program made = maker.callFunction("t_make_program", TenantCode);
check("module-made-program-flag", True, (made.getParseOptions() & PO_IN_MODULE).toBool());
check("module-made-program", made.getProgramId(), made.callFunction("t_probe"));

map printf("%s\n", $1), results;
# the file the module was loaded from, so that the test can tell the source module from its AOT module
printf("module-file=%s\n", basename(get_module_hash().CallerProbeA.filename));
