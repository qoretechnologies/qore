#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# Creates objects of classes whose base class constructor has a functional domain in Programs whose parse options
# restrict the domain after parsing; a class without a constructor of its own is checked as one with a constructor is:
# its base class constructors run in the class's Program with the options that the Program has now (qore#7513).
# The argument is the directory of the script (the test module RestrictionConstruction.qm), which a program compiled
# with AOT cannot find itself.

%modern
%no-child-restrictions

# classes of this Program, inherited by the child Programs below
public class PW inherits Socket {
}

public class PWC inherits Socket {
    constructor() {
    }
}

const ClassCode = "class W inherits Socket {}
class WC inherits Socket { constructor() {} }
class M inherits W {}
class MC inherits W { constructor() {} }
class MPW inherits PW { constructor() {} }
class LPW inherits PW {}
class MPWC inherits PWC { constructor() {} }
class LPWC inherits PWC {}
class B1 inherits W {}
class B2 inherits W {}
class D inherits B1, B2 {}
class HI { public { W w = new W(); } }
class HIC { public { W w = new W(); } constructor() {} }
string sub make(string cls) { object o = create_object(cls); return o.className(); }
string sub w() { W w(); return w.className(); }
string sub wnew() { W w = new W(); return w.className(); }
string sub wc() { WC w(); return w.className(); }
string sub m() { M w(); return w.className(); }
string sub mc() { MC w(); return w.className(); }
string sub pw() { PW w(); return w.className(); }
string sub pwc() { PWC w(); return w.className(); }
string sub mpw() { MPW w(); return w.className(); }
string sub lpw() { LPW w(); return w.className(); }
string sub mpwc() { MPWC w(); return w.className(); }
string sub lpwc() { LPWC w(); return w.className(); }
string sub diamond() { D w(); return w.className(); }
string sub member() { HI w(); return w.className(); }
string sub memberc() { HIC w(); return w.className(); }
string sub bg() {
    Queue q();
    background sub () {
        try {
            W w();
            q.push(w.className());
        } catch (hash<ExceptionInfo> ex) {
            q.push(ex.err);
        }
    }();
    return q.get();
}
";

const Calls = ("w", "wnew", "wc", "m", "mc", "pw", "pwc", "mpw", "lpw", "mpwc", "lpwc", "diamond", "member",
    "memberc", "bg");

# classes of a module, which inherit a class of the THREAD_CLASS domain
const ModuleCode = "%requires RestrictionConstruction
class XMT inherits MT {}
class XMTC inherits MT { constructor() {} }
string sub mt() { MT o(); return o.className(); }
string sub mtc() { MTC o(); return o.className(); }
string sub xmt() { XMT o(); return o.className(); }
string sub xmtc() { XMTC o(); return o.className(); }
";

const ModuleCalls = ("mt", "mtc", "xmt", "xmtc");

sub show(string label, Program p, string f) {
    try {
        printf("%s: %s: %s\n", label, f, p.callFunction(f));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, f, ex.err);
    }
}

sub run(string label, *SandboxManager sm) {
    Program p(PO_NEW_STYLE);
    if (sm) {
        p.setSandboxManager(sm);
    }
    p.parse(ClassCode, label);
    # before the restriction, every class can be instantiated
    foreach string f in (Calls) {
        show(label + " before", p, f);
    }
    p.setParseOptions(PO_NO_NETWORK);
    foreach string f in (Calls) {
        show(label, p, f);
    }
    # by name, at run time
    foreach string cls in ("W", "WC", "M", "MC") {
        try {
            printf("%s: create_object %s: %s\n", label, cls, p.callFunction("make", cls));
        } catch (hash<ExceptionInfo> ex) {
            printf("%s: create_object %s: %s: %s\n", label, cls, ex.err, ex.desc);
        }
    }
}

#! classes of a module: they run the constructors of their base classes in the module's Program, whether or not they
#! have a constructor of their own; for a sandboxed caller, with the caller's restrictions as well
sub run_module(string label, *SandboxManager sm) {
    Program p(PO_NEW_STYLE);
    if (sm) {
        p.setSandboxManager(sm);
    }
    p.parse(ModuleCode, label);
    p.setParseOptions(PO_NO_THREAD_CLASSES);
    foreach string f in (ModuleCalls) {
        show(label, p, f);
    }
}

string dir = absolute_path(ARGV[0]) ? ARGV[0] : getcwd() + "/" + ARGV[0];
load_module(dir + "/RestrictionConstruction.qm");

run("restricted");
run("sandboxed", new SandboxManager());
run_module("module restricted");
run_module("module sandboxed", new SandboxManager());
