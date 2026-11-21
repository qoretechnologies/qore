#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the checks of caller-restriction-grants.qtest of the objects and call references that the calling code creates, of
# the domains that a host grant lends and of the loading of native code: ARGV[0] is the path of the GrantPath module,
# which the host loads by its path, ARGV[1] the path of a native module outside the module path, ARGV[2] "restricted",
# "sandboxed" (restricted, with a sandbox; only the checks of run_common()) or "unrestricted"; prints one line per
# result

%modern
%no-child-restrictions
%requires GrantProbe
%requires GrantOther

load_module(ARGV[0]);
GrantProbe::set_native_path(ARGV[1]);

#! host code that the host imports into the Program: it runs with the host's Program on the stack
class HostClass {
    bool probe() {
        return SandboxManager::isCallerRestricted(DOM_EXTERNAL_INFO);
    }

    auto declared() {
        try {
            return GrantOther::declared();
        } catch (hash<ExceptionInfo> ex) {
            return ex.err;
        }
    }
}

bool sub host_probe() {
    return SandboxManager::isCallerRestricted(DOM_EXTERNAL_INFO);
}

int po = PO_NEW_STYLE;
if (ARGV[2] == "restricted") {
    po |= PO_NO_EXTERNAL_INFO | PO_NO_PROCESS_CONTROL | PO_NO_UNCONTROLLED_APIS;
} else if (ARGV[2] == "sandboxed") {
    # the module code that a sandboxed caller calls runs with its restrictions: without uncontrolled APIs, it could not
    # establish a grant
    po |= PO_NO_EXTERNAL_INFO;
}
Program p(po);
# a caller with a sandbox that only restricts its domains: the library code that it calls runs with its restrictions
if (ARGV[2] == "sandboxed") {
    SandboxManager sm();
    sm.setFilesystemDefaultPolicy(True);
    sm.setNetworkDefaultPolicy(True);
    p.setSandboxManager(sm);
}
p.importClass("HostClass");
p.importFunction("host_probe");
p.parse("%modern
%requires GrantProbe
%requires GrantPath
class Sub inherits GrantPath::Obj {
}
# trusted code under a grant that calls another library module, and the host's code that the host imported
hash<auto> sub run_common(string native) {
    hash<auto> h;
    h.cross_in_grant = GrantProbe::cross_in_grant();
    h.cross_in_capability = GrantProbe::cross_in_capability();
    h.cross_plain = GrantProbe::cross_plain();
    HostClass hc();
    h.imported_obj = hc.probe();
    h.imported_obj_ref = (\\hc.probe())();
    h.imported_obj_declared = hc.declared();
    h.imported_func_ref = (\\host_probe())();
    # the caller's own load of a native module: refused by the caller's options or by its sandbox's policy for
    # uncontrolled APIs (#7020), which are not functional-domain restrictions
    try {
        load_module(native);
        h.native_own = \"LOADED\";
    } catch (hash<ExceptionInfo> ex) {
        h.native_own = GrantProbe::classify_native_load(ex);
    }
    return h;
}
hash<auto> sub run(string native) {
    hash<auto> h = run_common(native);
    GrantPath::Obj o();
    Sub s();
    GrantPath::Obj made = GrantPath::make();
    h.path_direct = GrantPath::probe();
    h.closure_barrier = GrantProbe::call_barrier(bool sub () { return GrantPath::probe(); });
    h.path_ref_barrier = GrantProbe::call_barrier(\\GrantPath::probe());
    h.path_obj_ref_barrier = GrantProbe::call_barrier(\\o.probe());
    h.subclass_ref_barrier = GrantProbe::call_barrier(\\s.probe());
    h.made_obj_ref_barrier = GrantProbe::call_barrier(\\made.probe());
    h.builtin_ref_barrier = GrantProbe::call_barrier(\\SandboxManager::isCallerRestricted(), DOM_EXTERNAL_INFO);
    h.obj_with_grant = GrantProbe::with_grant(o);
    h.declared_ref_barrier = GrantProbe::try_barrier(\\o.viaHost());
    # not lent by a grant
    h.process_barrier = GrantProbe::barrier_process();
    # an object that the module creates under a grant does not record the lent restrictions; one created for the
    # caller does
    h.thing_made_in_grant = GrantProbe::thing_in_grant(GrantProbe::make_thing_in_grant());
    h.thing_made_plain = GrantProbe::thing_in_grant(GrantProbe::make_thing_plain());
    # a thread started under a grant: the lent restrictions are not passed on, the others are
    h.thread_in_grant = GrantProbe::thread_in_grant();
    # the restrictions are restored when an exception leaves a grant
    h.after_exception = GrantProbe::after_exception();
    # call references that the initialization of a library module calls
    GrantProbe::register(\\GrantPath::probe());
    GrantProbe::register(\\o.probe());
    load_module(\"GrantFire\");
    h.init_fired = call_function(\"GrantFire::fired\");
    # native code is loaded for the caller neither through a grant nor by the initialization of library code
    h.native_barrier = GrantProbe::call_barrier(auto sub () {
        try {
            load_module(native);
            return \"LOADED\";
        } catch (hash<ExceptionInfo> ex) {
            return GrantProbe::classify_native_load(ex);
        }
    });
    # library code under a host grant, in its own Program: decided by the caller walk without the grant (#6844)
    h.native_lib_barrier = GrantProbe::load_native_barrier(native);
    # under a host grant, into another Program: neither the grant nor the Program switch lends it
    h.native_program_barrier = GrantProbe::load_native_in_program_barrier(native);
    load_module(\"GrantNativeInit\");
    h.native_init = GrantProbe::get_native_init_result();
    return h;
}
", ARGV[2]);
# one line per result: the key, "=", and the value written with %y
map printf("%s=%y\n", $1.key, $1.value), (ARGV[2] == "sandboxed" ? p.callFunction("run_common", ARGV[1])
    : p.callFunction("run", ARGV[1])).pairIterator();

# a caller whose sandbox denies uncontrolled APIs while no parse option restricts them loads native code itself: only
# the sandbox's policy refuses it (#7020)
if (ARGV[2] == "sandboxed") {
    SandboxManager nsm();
    nsm.setUncontrolledApiPolicy(False);
    Program n(PO_NEW_STYLE);
    n.setSandboxManager(nsm);
    n.parse("%modern
%requires GrantProbe
string sub load(string path) {
    try {
        load_module(path);
        return \"LOADED\";
    } catch (hash<ExceptionInfo> ex) {
        return GrantProbe::classify_native_load(ex);
    }
}
", "native-sandbox");
    printf("native_sandbox_policy=%y\n", n.callFunction("load", ARGV[1]));
}

# the code of a trusted build runs for the Program that created the capability, also when that Program's own code
# calls it: a creator restricted only from external information (it may create the capability)
if (ARGV[2] != "sandboxed") {
    Program c(ARGV[2] == "restricted" ? PO_NEW_STYLE | PO_NO_EXTERNAL_INFO : PO_NEW_STYLE);
    c.parse("%modern
%requires GrantProbe
bool sub own_build() {
    code cap = SandboxManager::createTrustedBuildCall(bool sub () { return GrantProbe::plain(); });
    return cap();
}
", "build-creator");
    printf("own_build=%y\n", c.callFunction("own_build"));
}
