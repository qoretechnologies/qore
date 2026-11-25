#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# weak (:=) and opaque (@=) assignments of containers that the lvalue's type converts to a copy of their own, with
# types known only at run time: each is refused with RUNTIME-TYPE-ERROR and leaves the lvalue as it was, while the
# lvalues that keep the value store a reference to it (issue #7572)

%modern
%allow-weak-references
%allow-opaque-references

hashdecl WscHash {
    int a = 1;
}

class WscHolder {
    public {
        hash hh;
        any av;
        hash<auto> ha;
    }
}

our hash gh;
thread_local hash tlh;

#! stores a value weakly in a local that no closure captures, a hash, in a function that tiered execution compiles
int sub weak_store_error(auto v) {
    hash x = {};
    try {
        x := v;
    } catch (hash<ExceptionInfo> ex) {
        return 1;
    }
    return 0;
}

#! returns the number of errors of n calls of weak_store_error() with a typed hash
int sub weak_store_errors(int n) {
    int errs = 0;
    for (int i = 0; i < n; ++i) {
        errs += weak_store_error({"a": 1});
    }
    return errs;
}

sub try_store(string label, code store, code read) {
    try {
        store();
        printf("%s: stored: %y\n", label, read());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
        printf("%s: after: %y\n", label, read());
    }
}

# values whose type is known at run time only
auto src = {"a": 1};
auto lsrc = (1, 2);
auto dsrc = <WscHash>{};
auto empty = {};

{
    list l = (0,);
    try_store("untyped list element", sub () { l[0] := src; }, auto sub () { return l; });
    hash h = {"x": 0};
    try_store("untyped hash key", sub () { h.x := src; }, auto sub () { return h; });
    string k = "x";
    try_store("untyped hash key, dynamic", sub () { h{k} := src; }, auto sub () { return h; });
    any av = 0;
    try_store("any", sub () { av := src; }, auto sub () { return av; });
    hash hv = {};
    try_store("hash", sub () { hv := src; }, auto sub () { return hv; });
    list lv = ();
    try_store("list from list<int>", sub () { lv := lsrc; }, auto sub () { return lv; });
    try_store("hashdecl into hash", sub () { hv := dsrc; }, auto sub () { return hv; });
    try_store("hash<auto> into hash", sub () { hv := empty; }, auto sub () { return hv; });
    WscHolder o();
    try_store("opaque member", sub () { o.hh @= src; }, auto sub () { return o.hh; });
    try_store("opaque any member", sub () { o.av @= src; }, auto sub () { return o.av; });
    try_store("global", sub () { gh := src; }, auto sub () { return gh; });
    try_store("thread_local", sub () { tlh := src; }, auto sub () { return tlh; });
    *list vl;
    try_store("vivified list element", sub () { vl[0] := src; }, auto sub () { return vl; });
    list<softstring> lss = ();
    try_store("list<softstring> from list<int>", sub () { lss := lsrc; }, auto sub () { return lss; });
    hash rh = {};
    reference<hash> r = \rh;
    try_store("through a reference", sub () { r := src; }, auto sub () { return rh; });
}

# locals that no closure captures, which native code stores itself (qore#7572)
{
    hash x = {};
    try {
        x := src;
        printf("plain local hash: stored: %y\n", x);
    } catch (hash<ExceptionInfo> ex) {
        printf("plain local hash: %s; after: %y\n", ex.err, x);
    }
    any a = 0;
    try {
        a := src;
        printf("plain local any: stored: %y\n", a);
    } catch (hash<ExceptionInfo> ex) {
        printf("plain local any: %s; after: %y\n", ex.err, a);
    }
    list<softstring> ls = ();
    try {
        ls := lsrc;
        printf("plain local list<softstring>: stored: %y\n", ls);
    } catch (hash<ExceptionInfo> ex) {
        printf("plain local list<softstring>: %s; after: %y\n", ex.err, ls);
    }
    hash o = {};
    try {
        o @= src;
        printf("plain local opaque: stored: %y\n", o);
    } catch (hash<ExceptionInfo> ex) {
        printf("plain local opaque: %s; after: %y\n", ex.err, o);
    }
    printf("errors in a function called 2000 times: %d\n", weak_store_errors(2000));
}

# a reference: the value is stored in the referenced lvalue, whose type decides
{
    hash<string, int> th = {"a": 1};
    auto ax;
    reference rax = \ax;
    rax := th;
    hash<auto> hx;
    reference rhx = \hx;
    rhx := th;
    th.b = 2;
    printf("reference to auto: %y\n", ax);
    printf("reference to hash<auto>: %y\n", hx);
}

# the lvalues that keep the value: the weak reference designates it and sees its changes
{
    hash<string, int> th = {"a": 1};
    auto av;
    av := th;
    list<auto> la = ();
    la[0] := th;
    hash<auto> ha;
    ha := th;
    hash<string, int> mh;
    mh := th;
    WscHolder o();
    o.ha := th;
    hash ph = {"p": 1};
    hash phv;
    phv := ph;
    th.b = 2;
    ph.q = 2;
    printf("auto: %y\n", av);
    printf("list<auto> element: %y\n", la[0]);
    printf("hash<auto>: %y\n", ha);
    printf("matching type: %y\n", mh);
    printf("hash<auto> member: %y\n", o.ha);
    printf("plain hash into hash: %y\n", phv);
}
