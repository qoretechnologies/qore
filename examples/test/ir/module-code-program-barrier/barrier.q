#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

# calls every probe of BarrierMod as a module function and as a static method of a module class from a Program
# without restrictions, from one restricted only by its parse options, and from a sandboxed one with the same
# restrictions, and from the main Program; module-code-program-barrier.qtest compares the output in every execution
# mode, with the source and the AOT module

%modern
%no-child-restrictions
%requires BarrierMod

const CallerCode = "
auto sub r(code c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

hash<auto> sub probes() {
    hash<auto> h;
    h.\"function program\" = BarrierMod::func_program();
    h.\"method program\" = BarrierMod::Probe::program();
    h.\"function private function by name\" = BarrierMod::func_call_private();
    h.\"method private function by name\" = BarrierMod::Probe::callPrivate();
    h.\"function private class by name\" = BarrierMod::func_create_private();
    h.\"method private class by name\" = BarrierMod::Probe::createPrivate();
    h.\"function builtin by caller's name\" = BarrierMod::func_call_by_name(\"gethostname\");
    h.\"method builtin by caller's name\" = BarrierMod::Probe::callByName(\"gethostname\");
    h.\"function class by caller's name\" = BarrierMod::func_create_by_name(\"Mutex\");
    h.\"method class by caller's name\" = BarrierMod::Probe::createByName(\"Mutex\");
    h.\"function nested module code\" = BarrierMod::func_nested(\"gethostname\");
    h.\"method nested module code\" = BarrierMod::Probe::nested(\"gethostname\");
    h.\"function own thread data\" = BarrierMod::func_thread_data();
    h.\"method own thread data\" = BarrierMod::Probe::threadData();
    h.\"function async caller code\" = BarrierMod::func_async(string sub () { return \"async\"; });
    h.\"method async caller code\" = BarrierMod::Probe::async(string sub () { return \"async\"; });
    h.\"function callback\" = BarrierMod::func_callback(auto sub () {
        return r(string sub () { call_function(\"gethostname\"); return \"ok\"; });
    });
    h.\"method callback\" = BarrierMod::Probe::callback(auto sub () {
        return r(string sub () { call_function(\"gethostname\"); return \"ok\"; });
    });
    h.\"function object call reference\" = r(string sub () {
        auto l = BarrierMod::func_make_lock();
        code c = \\l.numReaders();
        return sprintf(\"readers: %d\", c());
    });
    h.\"method object call reference\" = r(string sub () {
        auto l = BarrierMod::Probe::makeLock();
        code c = \\l.numReaders();
        return sprintf(\"readers: %d\", c());
    });
    h.\"function call reference\" = r(string sub () { code c = \\BarrierMod::func_program(); return c(); });
    h.\"function call_function\" = r(string sub () { return call_function(\"BarrierMod::func_program\"); });
    return h;
}
";

const Restrictions = PO_NO_EXTERNAL_INFO | PO_NO_THREAD_CONTROL | PO_NO_THREAD_CLASSES;

# the module is loaded first by the main Program, which has no restrictions, so it can use everything it uses
hash<string, Program> callers;
callers.unrestricted = new Program(PO_NEW_STYLE);
callers."restricted by parse options" = new Program(PO_NEW_STYLE | Restrictions);
callers.sandboxed = new Program(PO_NEW_STYLE | Restrictions);
callers.sandboxed.setSandboxManager(new SandboxManager());

foreach hash<auto> i in (callers.pairIterator()) {
    i.value.setScriptPath("/caller/" + i.key);
    i.value.parse("%requires BarrierMod\n" + CallerCode, i.key);
}

# repeated calls run the code on every tier with tiered execution
for (int pass = 0; pass < 3; ++pass) {
    foreach hash<auto> i in (callers.pairIterator()) {
        hash<auto> h = i.value.callFunction("probes");
        if (pass) {
            continue;
        }
        foreach hash<auto> p in (h.pairIterator()) {
            printf("%s: %s: %y\n", i.key, p.key, p.value);
        }
    }
}

# the main Program calling the module directly, which compiled with AOT is a native call into the module
printf("main: function program: %y\n", BarrierMod::func_program());
printf("main: method program: %y\n", BarrierMod::Probe::program());
printf("main: function private function by name: %y\n", BarrierMod::func_call_private());
printf("main: method private function by name: %y\n", BarrierMod::Probe::callPrivate());
