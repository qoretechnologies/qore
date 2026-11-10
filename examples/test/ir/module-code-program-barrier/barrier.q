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
# names that the module's Program defines as well, and one that only the caller defines
string sub shared_name() {
    return \"caller\";
}

string sub caller_only() {
    return \"caller\";
}

class SharedClass {
    string who() {
        return \"caller\";
    }
}

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
    h.\"function caller-only name\" = BarrierMod::func_call_value(\"caller_only\");
    h.\"method caller-only name\" = BarrierMod::Probe::callValue(\"caller_only\");
    h.\"function same-named function\" = BarrierMod::func_call_value(\"shared_name\");
    h.\"method same-named function\" = BarrierMod::Probe::callValue(\"shared_name\");
    h.\"function same-named class\" = BarrierMod::func_create_who(\"SharedClass\");
    h.\"method same-named class\" = BarrierMod::Probe::createWho(\"SharedClass\");
    h.\"function closure from module code\" = r(BarrierMod::func_make_closure());
    h.\"method closure from module code\" = r(BarrierMod::Probe::makeClosure());
    h.\"closure from module code by name\" = r(BarrierMod::func_make_name_closure(\"shared_name\"));
    h.\"function call reference from module code\" = r(BarrierMod::func_make_ref());
    h.\"method call reference from module code\" = r(BarrierMod::Probe::makeRef());
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

# module code called by module code: ChainMod is loaded first by the sandboxed Program, so it has its restrictions
# itself; they still apply to the BarrierMod code that it calls for the sandboxed caller, but not for a caller
# restricted only by its parse options
callers.sandboxed.loadModule("ChainMod");
Program chain_s(PO_NEW_STYLE | Restrictions);
chain_s.setSandboxManager(new SandboxManager());
chain_s.parse("%requires ChainMod
auto sub f() { return ChainMod::create_through('Mutex'); }
auto sub m() { return ChainMod::Chain::createThrough('Mutex'); }", "chain sandboxed");
Program chain_r(PO_NEW_STYLE | Restrictions);
chain_r.parse("%requires ChainMod
auto sub f() { return ChainMod::create_through('Mutex'); }
auto sub m() { return ChainMod::Chain::createThrough('Mutex'); }", "chain restricted");
foreach hash<auto> i in ({"label": "sandboxed", "pgm": chain_s}, {"label": "restricted by parse options",
        "pgm": chain_r}) {
    printf("%s: function through a module: %y\n", i.label, i.pgm.callFunction("f"));
    printf("%s: method through a module: %y\n", i.label, i.pgm.callFunction("m"));
}

# a host calling a module function that a Program imported, in that Program: the call is made from the Program
foreach string label in ("restricted by parse options", "sandboxed") {
    printf("%s: host call of an imported function: %y\n", label,
        callers{label}.callFunction("BarrierMod::func_call_by_name", "gethostname"));
}

# module code running a sandboxed caller's reference to a function whose Program has been deleted
{
    Program defining(PO_NEW_STYLE);
    defining.parse("string sub defined() { return 'defined'; }
sub export_to(Program p) { p.importFunction('defined'); }", "defining");
    Program user(PO_NEW_STYLE | Restrictions);
    user.setSandboxManager(new SandboxManager());
    defining.callFunction("export_to", user);
    user.parse("%requires BarrierMod
auto sub run() { return BarrierMod::func_async(\\defined()); }", "user");
    printf("sandboxed: async reference to an imported function: %y\n", user.callFunction("run"));
    delete defining;
    printf("sandboxed: async reference to a function of a deleted Program: %y\n", user.callFunction("run"));
}

# concurrent calls into module code from several threads of a sandboxed and of an unrestricted Program; the
# arguments of the background call are evaluated in the calling thread
sub concurrent_call(Queue q, Program p, bool run) {
    try {
        if (run) {
            # every call ran in the module's Program, and the thread's counter in it counted only the thread's calls
            hash<auto> r = p.callFunction("run", 2000);
            q.push(r == {"module": True, "count": 4000} ? "ok" : sprintf("%y", r));
        } else {
            hash<auto> r = p.callFunction("probes");
            q.push(r."function program" == "module" && r."method program" == "module" ? "ok" : sprintf("%y", r));
        }
    } catch (hash<ExceptionInfo> ex) {
        q.push(ex.err);
    }
}

{
    Program c(PO_NEW_STYLE);
    c.parse("%requires BarrierMod
hash<auto> sub run(int n) {
    hash<auto> h;
    int count;
    for (int i = 0; i < n; ++i) {
        h{BarrierMod::func_program()} = True;
        h{BarrierMod::Probe::program()} = True;
        BarrierMod::func_thread_count();
        count = BarrierMod::Probe::threadCount();
    }
    h.count = count;
    return h;
}", "concurrent");
    Queue q();
    for (int t = 0; t < 4; ++t) {
        background concurrent_call(q, c, True);
        background concurrent_call(q, callers.sandboxed, False);
    }
    hash<string, int> results;
    for (int i = 0; i < 8; ++i) {
        ++results{q.get()};
    }
    printf("concurrent calls: %y\n", results);
}

# names that the module's Program defines as well, and one that only the main Program defines
string sub shared_name() {
    return "main";
}

string sub caller_only() {
    return "main";
}

class SharedClass {
    string who() {
        return "main";
    }
}

# the main Program calling the module directly, which compiled with AOT is a native call into the module
printf("main: function program: %y\n", BarrierMod::func_program());
printf("main: method program: %y\n", BarrierMod::Probe::program());
printf("main: function private function by name: %y\n", BarrierMod::func_call_private());
printf("main: method private function by name: %y\n", BarrierMod::Probe::callPrivate());
printf("main: function caller-only name: %y\n", BarrierMod::func_call_value("caller_only"));
printf("main: method same-named function: %y\n", BarrierMod::Probe::callValue("shared_name"));
printf("main: function same-named class: %y\n", BarrierMod::func_create_who("SharedClass"));
printf("main: closure from module code: %y\n", BarrierMod::func_make_closure()());
printf("main: call reference from module code: %y\n", BarrierMod::func_make_ref()());
