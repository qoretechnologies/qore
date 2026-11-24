#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the checks of caller-ambient-predicates.qtest: the host calls the probe for code of each kind; prints one line per
# result: the key, "=", and the value (%y)

%modern
%no-child-restrictions
%requires AmbientProbe

const Source = "%modern
%requires AmbientProbe
string sub direct() {
    return AmbientProbe::probe();
}
string sub lent(AmbientProbe::Lent l) {
    return l.probe();
}
bool sub lent_restricted(AmbientProbe::Lent l) {
    return l.callerRestricted();
}
string sub lent_barrier(AmbientProbe::Lent l) {
    return l.barrierProbe();
}
string sub barrier() {
    return AmbientProbe::barrier_probe();
}
# its own callback that a host grant calls: still its own code
string sub barrier_callback() {
    return AmbientProbe::barrier_callback(string sub () { return AmbientProbe::probe(); });
}
string sub capability(code cap) {
    return cap();
}
# a call reference to library code that it creates itself, which the host calls later
code sub own_ref() {
    return \\AmbientProbe::probe();
}
# a call reference that library code creates for it
code sub library_ref() {
    return AmbientProbe::make_ref();
}
";

# the kinds of calling code: their parse options and whether a sandbox governs them
const Kinds = {
    "unrestricted": {"po": 0, "sandboxed": False},
    "no_external_info": {"po": PO_NO_EXTERNAL_INFO, "sandboxed": False},
    # lockdown, except that it may use the module that the host loaded
    "lockdown": {"po": PO_LOCKDOWN & ~PO_NO_MODULES, "sandboxed": False},
    "other_restriction": {"po": PO_NO_NETWORK | PO_NO_PROCESS_CONTROL, "sandboxed": False},
    "sandboxed": {"po": 0, "sandboxed": True},
    "sandboxed_no_external_info": {"po": PO_NO_EXTERNAL_INFO, "sandboxed": True},
};

hash<string, auto> sub run(string kind, hash<auto> k, code cap, Qore::ExecMode mode) {
    Program p(PO_NEW_STYLE | k.po);
    # runtime-created Programs do not take the execution mode of the process
    p.setExecMode(mode);
    if (k.sandboxed) {
        # a sandbox that allows everything: it governs the code without restricting it
        SandboxManager sm();
        sm.setNetworkDefaultPolicy(True);
        p.setSandboxManager(sm);
    }
    p.parse(Source, kind);
    hash<string, auto> h;
    h.direct = p.callFunction("direct");
    AmbientProbe::Lent l();
    h.lent = p.callFunction("lent", l);
    h.lent_restricted = p.callFunction("lent_restricted", l);
    h.lent_barrier = p.callFunction("lent_barrier", l);
    h.barrier = p.callFunction("barrier");
    h.barrier_callback = p.callFunction("barrier_callback");
    h.capability = p.callFunction("capability", cap);
    code own = p.callFunction("own_ref");
    h.own_ref = own();
    code lib = p.callFunction("library_ref");
    h.library_ref = lib();
    return h;
}

# the host's own code
printf("host_direct=%y\n", AmbientProbe::probe());
printf("host_barrier=%y\n", AmbientProbe::barrier_probe());
# a capability that the host registers before any other code runs
code cap = SandboxManager::createSystemPolicyCall(\AmbientProbe::probe());
printf("host_capability=%y\n", cap());
# the code of the calling Programs in every execution mode
foreach hash<auto> m in ({"ast": EM_AST, "ir": EM_IR, "jit": EM_JIT, "tiered": EM_TIERED}.pairIterator()) {
    foreach hash<auto> i in (Kinds.pairIterator()) {
        foreach hash<auto> r in (run(i.key, i.value, cap, m.value).pairIterator()) {
            printf("%s_%s_%s=%y\n", m.key, i.key, r.key, r.value);
        }
    }
}
