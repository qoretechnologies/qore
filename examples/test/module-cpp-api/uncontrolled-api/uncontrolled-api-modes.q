#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (C) 2026 Qore Technologies, s.r.o.

# Runs the uncontrolled-API probe calls in a sandboxed Program with the execution mode given (qore#6167); see
# uncontrolled-api-probe.qtest, which runs it in every mode
#
# usage: uncontrolled-api-modes.q <ast|ir|jit|tiered> [module] | --module-source
#
# A Program created at run time does not take the --exec-mode of the process, so the mode is set on it with
# Program::setExecMode(); its code is %modern, which the compiled modes require.  Every call is made by a function and
# by the top-level code of the Program:
# - call_functions() calls each function twice and records the second calls, which run the native code of the
#   functions with --exec-mode=jit and --exec-mode=tiered (and the synchronous compilation of ExecModeHarness), as a
#   debugger shows; see call_functions()
# - the top-level code makes the same calls itself: it runs as native code with --exec-mode=jit, and on the IR tier
#   with --exec-mode=tiered
# Each kind of call is covered: a static method, a function and a constructor, whose variants are found when the
# calls are parsed, a call through a call reference, and a method call on a typed and on an untyped receiver (whose
# variant is found when the call is made); the receivers are probes that trusted code created and gave to the
# Program.  With "module", the functions are those of the UncontrolledProbeCalls module (its source is printed with
# --module-source), which can be a compiled (AOT) module.

%modern
%no-child-restrictions

%requires uncontrolledprobe

const Modes = {
    "ast": EM_AST,
    "ir": EM_IR,
    "jit": EM_JIT,
    "tiered": EM_TIERED,
};

#! the functions that make the calls directly in their bodies; each one returns the result, or the error
const Functions = "
auto sub typed_method_default(UncontrolledProbe::Probe p) {
    try {
        return p.withDefault();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
auto sub method_default(object p) {
    try {
        return p.withDefault();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
auto sub static_default() {
    try {
        return UncontrolledProbe::Probe::staticWithDefault();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
auto sub function_default() {
    try {
        return UncontrolledProbe::call();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
auto sub call_ref_default() {
    try {
        code c = \\UncontrolledProbe::call();
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
auto sub construct() {
    try {
        UncontrolledProbe::Probe p();
        return \"constructed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
";

#! the top-level code: calls the functions, then makes the same calls itself
const TopLevel = "
# the probes that trusted code created: a typed and an untyped receiver
our *UncontrolledProbe::Probe probe;
our *object untyped_probe;
our hash<auto> functions;
our hash<auto> top;

# calls each function twice and returns the results of the second calls, which reach the native code of each function:
# a function with arguments takes the call path, which runs it on the IR tier and compiles it when it returns, so its
# second call runs its native code; with JIT execution this function is native code itself (the top-level code calls
# it without arguments, through the fast-call path, which compiles it before it runs), and with tiered execution the
# IR interpreter runs it inline, but its calls still reach the compiled functions (qore_rt_call_fast, evalTiered):
# the IR interpreter does not inline a function into a function that it runs inline, as it inlines a small function
# that the top-level code calls directly, which is why the top-level code does not call them itself
hash<auto> sub call_functions() {
    hash<auto> rv;
    for (int i = 0; i < 2; ++i) {
        rv = {
            \"typed_method_default\": typed_method_default(probe),
            \"method_default\": method_default(untyped_probe),
            \"static_default\": static_default(),
            \"function_default\": function_default(),
            \"call_ref_default\": call_ref_default(),
            \"construct\": construct(),
        };
    }
    return rv;
}

functions = call_functions();

try {
    top.typed_method_default = probe.withDefault();
} catch (hash<ExceptionInfo> ex) {
    top.typed_method_default = ex.err;
}
try {
    top.method_default = untyped_probe.withDefault();
} catch (hash<ExceptionInfo> ex) {
    top.method_default = ex.err;
}
try {
    top.static_default = UncontrolledProbe::Probe::staticWithDefault();
} catch (hash<ExceptionInfo> ex) {
    top.static_default = ex.err;
}
try {
    top.function_default = UncontrolledProbe::call();
} catch (hash<ExceptionInfo> ex) {
    top.function_default = ex.err;
}
try {
    code c = \\UncontrolledProbe::call();
    top.call_ref_default = c();
} catch (hash<ExceptionInfo> ex) {
    top.call_ref_default = ex.err;
}
try {
    UncontrolledProbe::Probe p();
    top.construct = \"constructed\";
} catch (hash<ExceptionInfo> ex) {
    top.construct = ex.err;
}
";

*string mode_name = ARGV[0];
if (mode_name == "--module-source") {
    # the source of the UncontrolledProbeCalls module: the functions above, public
    printf("%%modern\n%%requires uncontrolledprobe\n\nmodule UncontrolledProbeCalls {\n    version = \"1.0\";\n"
        "    desc = \"the uncontrolled-API probe calls (qore#6167)\";\n    author = \"Qore Technologies, s.r.o.\";\n"
        "    license = \"MIT\";\n}\n%s", regex_subst(Functions, "^auto sub ", "public auto sub ",
        RE_MultiLine | RE_Global));
    exit(0);
}
if (!exists Modes{mode_name}) {
    stderr.printf("usage: %s <%s> [module] | --module-source\n", get_script_name(), Modes.keys().join("|"));
    exit(1);
}
# "module": the functions are those of the UncontrolledProbeCalls module, which the process's module path
# (QORE_MODULE_DIR) must find: a module that code that a sandbox governs loads from outside the module path is bound
# to that sandbox, and the Programs here have different sandboxes
bool use_module = ARGV[1] == "module";

#! returns a sandboxed Program with the given policy and the execution mode, with the calls parsed
Program sub get_program(bool allow) {
    Program p(PO_MODERN);
    p.setExecMode(Modes{mode_name});
    SandboxManager sm();
    if (allow) {
        sm.setUncontrolledApiPolicy(True);
    }
    p.setSandboxManager(sm);
    p.parse((use_module ? "%requires UncontrolledProbeCalls\n" : Functions) + TopLevel, "uncontrolled-api-modes");
    return p;
}

# the probes are created by trusted code, which no sandbox governs
UncontrolledProbe::Probe probe();
UncontrolledProbe::Probe untyped_probe();
foreach bool allow in ((False, True)) {
    string label = sprintf("%s %s", mode_name, allow ? "allowed" : "refused");
    int count = UncontrolledProbe::SideEffect::count();
    Program p = get_program(allow);
    p.setGlobalVarValue("probe", probe);
    p.setGlobalVarValue("untyped_probe", untyped_probe);
    p.run();
    printf("%s functions: %y\n", label, p.getGlobalVariable("functions"));
    printf("%s top-level: %y\n", label, p.getGlobalVariable("top"));
    # a refused call evaluates no default argument; each allowed call with a default argument evaluates one
    printf("%s default arguments evaluated: %d\n", label, UncontrolledProbe::SideEffect::count() - count);
}
