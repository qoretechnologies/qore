#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# The functions of weak-container-receiver-lifetime.qtest: each calls a method on an object read through a weak
# reference that is not a := local - the variable of a foreach loop whose element is a weak reference, or a weak
# reference held by a hash - to an object whose only strong reference is in g_h, which the method removes; the
# destructor logs where the object is released (qore#6594).  The receiver read from a foreach variable is kept until
# the call returns; the one read from a hash member is a temporary of the statement, kept until the statement ends

%modern
%allow-weak-references

string log;

class T {
    public { string n; bool raise; }
    constructor(string n, bool raise = False) { self.n = n; self.raise = raise; }
    destructor() { log += "~" + n; if (raise) { throw "DTOR", n; } }
    string clear() { remove g_h{n}; log += "|m"; return n; }
    string clearRaise() { remove g_h{n}; log += "|r" + n; throw "ERR", n; }
    string keep() { log += "|k"; return n; }
}

class Holder {
    public {
        auto w;
        static auto sw;
    }

    # a weak reference held by a member of the current object, read without self.
    selfMember() {
        put("a");
        w := g_h.a;
        mark(w.clear());
        log += "|e";
    }

    # the same in a try block
    selfMemberInTry() {
        put("a");
        w := g_h.a;
        try {
            mark(w.clear());
            log += "|e";
        } catch (hash<ExceptionInfo> ex) {
            log += "|c" + ex.err;
        }
    }

    # a static class variable in a try block
    staticMemberInTry() {
        put("a");
        sw := g_h.a;
        try {
            mark(sw.clear());
            log += "|e";
        } catch (hash<ExceptionInfo> ex) {
            log += "|c" + ex.err;
        }
        remove sw;
    }

    # the same, read with self.
    selfDotMember() {
        put("a");
        w := g_h.a;
        mark(self.w.clear());
        log += "|e";
    }

    # a weak reference held by a static class variable
    staticMember() {
        put("a");
        sw := g_h.a;
        mark(sw.clear());
        log += "|e";
        remove sw;
    }
}

hashdecl WeakHolder {
    T o;
    int x = 1;
}

our hash<string, T> g_h;

#! a global variable that holds a weak reference
our *T g_w;

auto sub mark(auto v) { log += "|p" + v; return v; }

sub put(string n, bool raise = False) { g_h{n} = new T(n, raise); }

sub report(string name) { printf("%s: %s\n", name, log); log = ""; }

# logs an exception that a function does not handle
sub unhandled(hash<ExceptionInfo> ex) { log += "|u" + ex.err; }

# reports the log of a function; an object that the function left in g_h is destroyed here, in its own report
sub finish(string name) {
    if (g_h) {
        log += "|g";
        remove g_h;
    }
    report(name);
}

# a list whose elements are weak references
list<auto> sub weakList(list<string> names) {
    list<auto> l = ();
    foreach string n in (names) {
        put(n);
        l[l.size()] := g_h{n};
    }
    return l;
}

# a typed foreach variable over weak references
sub foreachTyped() {
    foreach T w in (weakList(("a", "b"))) {
        mark(w.clear());
        log += "|n";
    }
    log += "|e";
}

# an untyped foreach variable over weak references
sub foreachUntyped() {
    foreach auto w in (weakList(("a", "b"))) {
        mark(w.clear());
        log += "|n";
    }
    log += "|e";
}

# over a list in a local variable
sub foreachLocalList() {
    put("a");
    list<auto> l = ();
    l[0] := g_h.a;
    foreach T w in (l) {
        mark(w.clear());
    }
    log += "|e";
}

# in a try block
sub foreachInTry() {
    try {
        foreach T w in (weakList(("a",))) {
            mark(w.clear());
        }
        log += "|e";
    } catch (hash<ExceptionInfo> ex) {
        log += "|c" + ex.err;
    }
}

# a list holding weak references and strong values: a strong element is kept by the list
sub foreachMixed() {
    list<auto> l = weakList(("a",));
    put("b");
    l += g_h.b;
    put("c");
    l[l.size()] := g_h.c;
    foreach auto w in (l) {
        mark(w.clear());
    }
    log += "|e";
    remove l;
    log += "|f";
}

# an untyped variable whose values are nodes that are not weak references (a string, a hash, an object)
sub foreachNodes() {
    put("a");
    foreach auto v in (("x", {"k": "y"}, g_h.a)) {
        if (v.typeCode() == NT_OBJECT) {
            mark(v.keep());
        } else if (v.typeCode() == NT_HASH) {
            mark(v.k);
        } else {
            mark(v.size());
        }
    }
    remove g_h.a;
    log += "|e";
}

# the method raises: the exception releases the receiver
sub foreachRaising() {
    try {
        foreach T w in (weakList(("a",))) {
            mark(w.clearRaise());
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "|c" + ex.err;
    }
    log += "|e";
}

# the method does not release the object
sub foreachKept() {
    foreach T w in (weakList(("a",))) {
        mark(w.keep());
    }
    remove g_h.a;
    log += "|e";
}

# the object was deleted before the call
sub foreachDeleted() {
    list<auto> l = weakList(("a",));
    remove g_h.a;
    try {
        foreach T w in (l) {
            mark(w.keep());
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "|c" + ex.err;
    }
    log += "|e";
}

# the destructor of the receiver raises when the call returns
sub foreachDtorRaise() {
    list<auto> l = ();
    put("a", True);
    l[0] := g_h.a;
    try {
        foreach T w in (l) {
            mark(w.clear());
            log += "|x";
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "|c" + ex.err;
    }
    log += "|e";
}

# a weak reference held by a hash: kept until the end of the statement
sub hashMember() {
    put("a");
    hash<auto> h = {};
    h.o := g_h.a;
    mark(h.o.clear());
    log += "|e";
}

# the same in a try block
sub hashMemberInTry() {
    put("a");
    hash<auto> h = {};
    h.o := g_h.a;
    try {
        mark(h.o.clear());
        log += "|e";
    } catch (hash<ExceptionInfo> ex) {
        log += "|c" + ex.err;
    }
}

# a weak reference in a nested hash
sub hashNested() {
    put("a");
    hash<auto> h = {"a": {}};
    h.a.b := g_h.a;
    mark(h.a.b.clear());
    log += "|e";
}

# a weak reference held by a hashdecl member
sub hashDeclMember() {
    put("a");
    hash<WeakHolder> h();
    h.o := g_h.a;
    mark(h.o.clear());
    log += "|e";
}

# the statement makes other calls after the method: the object lives until all of them have run
sub hashMemberStatement() {
    put("a");
    hash<auto> h = {};
    h.o := g_h.a;
    mark(mark(h.o.clear()) + mark("q"));
    log += "|e";
}

# in a loop
sub hashMemberLoop() {
    hash<auto> h = {};
    foreach string n in (("a", "b")) {
        put(n);
        h.o := g_h{n};
        mark(h.o.clear());
        log += "|n";
    }
    log += "|e";
}

# the method raises: the object is released with the temporaries of the statement
sub hashMemberRaising() {
    put("a");
    hash<auto> h = {};
    h.o := g_h.a;
    try {
        mark(h.o.clearRaise());
    } catch (hash<ExceptionInfo> ex) {
        log += "|c" + ex.err;
    }
    log += "|e";
}

# the method raises, with no handler in the function
sub hashMemberRaisingOut() {
    put("a");
    hash<auto> h = {};
    h.o := g_h.a;
    mark(h.o.clearRaise());
}

sub hashMemberRaisingCaller() {
    try {
        hashMemberRaisingOut();
    } catch (hash<ExceptionInfo> ex) {
        log += "|c" + ex.err;
    }
    log += "|e";
}

# the method does not release the object
sub hashMemberKept() {
    put("a");
    hash<auto> h = {};
    h.o := g_h.a;
    mark(h.o.keep());
    remove g_h.a;
    log += "|e";
}

# the object was deleted before the call
sub hashMemberDeleted() {
    put("a");
    hash<auto> h = {};
    h.o := g_h.a;
    remove g_h.a;
    try {
        mark(h.o.keep());
    } catch (hash<ExceptionInfo> ex) {
        log += "|c" + ex.err;
    }
    log += "|e";
}

# the destructor of the receiver raises when the statement ends
sub hashMemberDtorRaise() {
    put("a", True);
    hash<auto> h = {};
    h.o := g_h.a;
    try {
        mark(h.o.clear());
        log += "|x";
    } catch (hash<ExceptionInfo> ex) {
        log += "|c" + ex.err;
    }
    log += "|e";
}

# a weak reference held by a member of an object: kept until the end of the statement
sub objectMember() {
    put("a");
    Holder o();
    o.w := g_h.a;
    mark(o.w.clear());
    log += "|e";
}

# a weak reference read from a hash with a key expression: kept until the end of the statement
sub hashKeyExpression() {
    put("a");
    hash<auto> h = {};
    h.o := g_h.a;
    string k = "o";
    mark(h{k}.clear());
    log += "|e";
}

# a weak reference held by a global variable: kept until the end of the statement
sub globalVar() {
    put("a");
    g_w := g_h.a;
    mark(mark(g_w.clear()) + mark("q"));
    log += "|e";
    remove g_w;
}

# a weak reference held by a list element: kept until the end of the statement
sub listElement() {
    put("a");
    list<auto> l = ();
    l[0] := g_h.a;
    mark(l[0].clear());
    log += "|e";
}

# a function with many statements, whose groups qcc can outline
sub large(int x) {
    if (x > 0) {
        foreach T w in (weakList(("a",))) {
            mark(w.clear());
        }
        log += "|1";
    } else {
        log += "|0";
    }
    for (int i = 0; i < 2; ++i) {
        log += "|i" + i;
    }
    hash<auto> h = {};
    put("b");
    h.o := g_h.b;
    if (x > 1) {
        mark(h.o.clear());
        log += "|2";
    } else {
        log += "|0";
    }
    for (int i = 0; i < 2; ++i) {
        log += "|j" + i;
    }
    put("c");
    h.o := g_h.c;
    if (x > 2) {
        try {
            mark(h.o.clearRaise());
        } catch (hash<ExceptionInfo> ex) {
            log += "|c" + ex.err;
        }
        log += "|3";
    } else {
        log += "|0";
    }
    for (int i = 0; i < 2; ++i) {
        log += "|k" + i;
    }
    log += "|e";
}

try { foreachTyped(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("foreachTyped");
try { foreachUntyped(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("foreachUntyped");
try { foreachLocalList(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("foreachLocalList");
try { foreachInTry(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("foreachInTry");
try { foreachMixed(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("foreachMixed");
try { foreachNodes(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("foreachNodes");
try { foreachRaising(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("foreachRaising");
try { foreachKept(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("foreachKept");
try { foreachDeleted(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("foreachDeleted");
try { foreachDtorRaise(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("foreachDtorRaise");
try { hashMember(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("hashMember");
try { hashMemberInTry(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("hashMemberInTry");
try { hashNested(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("hashNested");
try { hashDeclMember(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("hashDeclMember");
try { hashMemberStatement(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("hashMemberStatement");
try { hashMemberLoop(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("hashMemberLoop");
try { hashMemberRaising(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("hashMemberRaising");
try { hashMemberRaisingCaller(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("hashMemberRaisingCaller");
try { hashMemberKept(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("hashMemberKept");
try { hashMemberDeleted(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("hashMemberDeleted");
try { hashMemberDtorRaise(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("hashMemberDtorRaise");
try { objectMember(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("objectMember");
try { hashKeyExpression(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("hashKeyExpression");
try { globalVar(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("globalVar");
try { listElement(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("listElement");
try { Holder o(); o.selfMember(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("selfMember");
try { Holder o(); o.selfMemberInTry(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("selfMemberInTry");
try { Holder o(); o.staticMemberInTry(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("staticMemberInTry");
try { Holder o(); o.selfDotMember(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("selfDotMember");
try { Holder o(); o.staticMember(); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("staticMember");
try { large(3); } catch (hash<ExceptionInfo> ex) { unhandled(ex); }
finish("large");
