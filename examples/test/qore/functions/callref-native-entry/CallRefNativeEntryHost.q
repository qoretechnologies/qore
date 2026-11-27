#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# Calls through references in the code of a module compiled with qcc (qore#5578): the module's code calls references
# to its own functions through their call-reference entries in the module's Program, for the script and for another
# Program that loads the module; references that the script made and the module's references that the script calls
# take the generic call path.  The results and exceptions must be those of the module loaded from its source with the
# AST interpreter.
#
# Prints a line for each case.  With the argument "report" (debug builds), it prints after each case whether its calls
# took a call-reference entry ("native"), the runtime's fast call through a reference ("fast") or neither ("generic").

%modern
%no-child-restrictions
%requires CallRefNativeEntryModule

const N = 200;

bool report = ARGV[0] == "report";

int sub host_step(int value) {
    return (value * 33 + 17) & 65535;
}

int native_calls_before = 0;
int fast_calls_before = 0;

sub run_case(string name, code c) {
    # the report reads the counts of a debug build of the library
    if (report && Option::HAVE_LIBRARY_DEBUGGING) {
        native_calls_before = call_function("dbg_function_reference_native_calls");
        fast_calls_before = call_function("dbg_function_reference_fast_calls");
    }
    string result;
    try {
        result = c();
    } catch (hash<ExceptionInfo> ex) {
        result = sprintf("unexpected %s: %s", ex.err, ex.desc);
    }
    # the report reads the counts of a debug build of the library
    if (report && Option::HAVE_LIBRARY_DEBUGGING) {
        int native = call_function("dbg_function_reference_native_calls") - native_calls_before;
        int fast = call_function("dbg_function_reference_fast_calls") - fast_calls_before;
        printf("%s: %s\n", name, native ? "native" : (fast ? "fast" : "generic"));
    } else {
        printf("%s: %s\n", name, result);
    }
}

run_case("module's own reference", string sub () { return sprintf("%d", mod_loop(N, 7)); });
run_case("module's own float reference", string sub () { return sprintf("%.3f", mod_loop_float(N, 7.0)); });
run_case("module's exception", string sub () { return mod_exception(5); });
run_case("module's reference given back", string sub () { return sprintf("%d", mod_apply(mod_ref(), N, 7)); });
run_case("script's reference called by the module", string sub () {
    return sprintf("%d", mod_apply(\host_step(), N, 7));
});
run_case("module's reference called by the script", string sub () {
    code<int(int)> f = mod_ref();
    int v = 7;
    for (int i = 0; i < N; ++i) {
        v = f(v);
    }
    return sprintf("%d", v);
});
run_case("another Program", string sub () {
    Program p(PO_NEW_STYLE);
    p.loadModule("CallRefNativeEntryModule");
    p.parse("int sub t(int n) { return mod_loop(n, 7) + mod_apply(mod_ref(), n, 9); }", "child");
    return sprintf("%d", p.callFunction("t", N));
});

# the sandbox: a Program with a sandbox manager that loads the module, references handed between it and the script,
# and a reference that the module's code makes for a governed call; the references that the module's code makes for
# the sandboxed Program record the sandbox's provenance, so they take the generic call path, as do the references
# made by the code of another Program and the reference made for a governed call; only a reference made without
# provenance, in the context of its call, uses its entry (its body, context-independent, calls no builtin, so it
# observes no restriction that the generic call path would apply).  A caller's PO_NO_FILESYSTEM does not reach module
# code (a module keeps its own filesystem and network options, and the sandbox governs that access at run time), so
# the module code runs with its own options under both callers, the reference's creator context is the current one,
# and the call uses the entry; a caller's PO_NO_THREAD_CONTROL is applied to the module's code, whose effective parse
# options then differ from its creator's, so that call takes the generic call path
SandboxManager manager();
Program sandboxed(PO_NEW_STYLE);
sandboxed.setSandboxManager(manager);
sandboxed.loadModule("CallRefNativeEntryModule");
sandboxed.parse("int sub sb_loop(int n) { return mod_loop(n, 7) + mod_apply(mod_ref(), n, 9); }
int sub sb_apply(code<int(int)> f, int n) { return mod_apply(f, n, 7); }
code<int(int)> sub sb_ref() { return mod_ref(); }", "sandboxed");
run_case("sandboxed Program's module calls", string sub () {
    return sprintf("%d", sandboxed.callFunction("sb_loop", N));
});
run_case("script's module reference called in the sandboxed Program", string sub () {
    return sprintf("%d", sandboxed.callFunction("sb_apply", mod_ref(), N));
});
run_case("sandboxed Program's module reference called by the script", string sub () {
    return sprintf("%d", mod_apply(sandboxed.callFunction("sb_ref"), N, 7));
});
run_case("module reference made for a governed call", string sub () {
    code<int(int)> f = SandboxManager::callGoverned(\mod_ref(), sandboxed);
    return sprintf("%d", mod_apply(f, N, 7));
});
Program restricted(PO_NEW_STYLE | PO_NO_FILESYSTEM);
restricted.setSandboxManager(manager);
restricted.loadModule("CallRefNativeEntryModule");
restricted.parse("int sub r_apply(code<int(int)> f, int n) { return mod_apply(f, n, 7); }", "restricted");
run_case("script's module reference called in a Program with parse restrictions", string sub () {
    return sprintf("%d", restricted.callFunction("r_apply", mod_ref(), N));
});
Program restricted_threads(PO_NEW_STYLE | PO_NO_THREAD_CONTROL);
restricted_threads.setSandboxManager(manager);
restricted_threads.loadModule("CallRefNativeEntryModule");
restricted_threads.parse("int sub t_apply(code<int(int)> f, int n) { return mod_apply(f, n, 7); }",
    "restricted_threads");
run_case("script's module reference called in a Program without thread control", string sub () {
    return sprintf("%d", restricted_threads.callFunction("t_apply", mod_ref(), N));
});
