%modern
%no-child-restrictions

# the program of sandbox-uncontrolled-api-refusal.qtest (testModes(), testCompiled()): every case prints one line,
# which the test compares in every execution mode and as a qcc executable.  The sandboxed Programs run the code that
# they parse at run time; the closures of this program, which is compiled by qcc, run as compiled code for them

#! the code of the sandboxed Programs
const Source = "
auto sub run(code c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
auto sub direct() { return run(sub () { return SandboxManager::isGoverned(); }); }
auto sub call_ref() { return run(sub () { code c = \\SandboxManager::getGoverningPolicies(); return c().size(); }); }
auto sub own_barrier() { return run(sub () { return SandboxManager::callWithSystemPolicy(sub () { return 1; }); }); }
auto sub runtime_variant() { return run(sub () { auto f = \"isGoverned\"; return call_static_method(\"SandboxManager\", f); }); }
auto sub capability() { return run(sub () { return SandboxManager::createSystemPolicyCall(sub () {}) ? \"created\" : \"none\"; }); }
code sub make_ref() { return \\SandboxManager::isGoverned(); }
auto sub through(code c) { return run(c); }
auto sub background_direct() {
    Queue q();
    background q.push(direct());
    return q.get();
}
";

#! returns what to print for a result
string sub show(auto v) {
    return v.typeCode() == NT_STRING ? v : sprintf("%y", v);
}

#! runs \a c in this program and returns the result or the error
auto sub run(code c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! returns a Program with Source and a sandbox with the given policy, or with no sandbox
Program sub make_program(*bool allow) {
    Program p(PO_NEW_STYLE);
    if (exists allow) {
        SandboxManager sm();
        if (allow) {
            sm.setUncontrolledApiPolicy(True);
        }
        p.setSandboxManager(sm);
    }
    p.parse(Source, "refusal-matrix");
    return p;
}

Program deny = make_program(False);
Program allow = make_program(True);
Program trusted = make_program();

# the sandboxed code's own calls: refused, however the call is made
printf("01 sandboxed, direct:             %s\n", show(deny.callFunction("direct")));
printf("02 sandboxed, call reference:     %s\n", show(deny.callFunction("call_ref")));
printf("03 sandboxed, own barrier:        %s\n", show(deny.callFunction("own_barrier")));
printf("04 sandboxed, run-time variant:   %s\n", show(deny.callFunction("runtime_variant")));
printf("05 sandboxed, capability:         %s\n", show(deny.callFunction("capability")));
printf("06 sandboxed, thread:             %s\n", show(deny.callFunction("background_direct")));
# a call reference that the sandboxed code created is judged by its Program, also when this program calls it
{
    code ref = deny.callFunction("make_ref");
    printf("07 sandboxed reference, here:     %s\n", show(run(ref)));
}
# the code of this program (compiled code under qcc) called by the sandboxed code: the introspection and barrier
# methods are judged by this program, which is not sandboxed; any other uncontrolled API is refused
printf("08 here for sandboxed, governed:  %s\n", show(deny.callFunction("through",
    auto sub () { return SandboxManager::isGoverned(); })));
printf("09 here for sandboxed, policies:  %s\n", show(deny.callFunction("through",
    auto sub () { return SandboxManager::getGoverningPolicies()[0].uncontrolled_api_policy; })));
{
    # the call is created before the sandboxed code runs: a closure created while serving it carries its sandbox
    code system_call = auto sub () { return SandboxManager::isGoverned(); };
    printf("10 here for sandboxed, barrier:   %s\n", show(deny.callFunction("through",
        auto sub () { return SandboxManager::callWithSystemPolicy(system_call); })));
}
printf("11 here for sandboxed, capability: %s\n", show(deny.callFunction("through",
    auto sub () { return SandboxManager::createSystemPolicyCall(sub () {}) ? "created" : "none"; })));
# a sandbox that allows uncontrolled APIs, and no sandbox
printf("12 trusting sandbox, direct:      %s\n", show(allow.callFunction("direct")));
printf("13 trusting sandbox, own barrier: %s\n", show(allow.callFunction("own_barrier")));
printf("14 no sandbox, direct:            %s\n", show(trusted.callFunction("direct")));
printf("15 no sandbox, capability:        %s\n", show(trusted.callFunction("capability")));
