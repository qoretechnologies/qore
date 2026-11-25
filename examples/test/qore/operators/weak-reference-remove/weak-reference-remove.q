#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# remove and delete of an element, a key, a slice or a range through a weak reference that an lvalue holds change
# the list or hash that the weak reference designates, as an assignment through it does (qore#7306); see
# ../weak-reference-remove.qtest

%modern
%allow-weak-references
%no-child-restrictions

class D {
    public {
        string n;
        hash<auto> holder;
    }

    constructor(string n) {
        self.n = n;
    }

    destructor() {
        printf("destroy %s\n", n);
    }
}

class M {
    public {
        list<auto!> wl;
        hash<auto!> wh;
    }
}

hashdecl HD {
    int a = 1;
    *int b = 2;
}
class S {
    public {
        static list<auto!> swl;
        static hash<auto!> swh;
    }
}

our list<auto!> gwl;

sub err(string label, hash<ExceptionInfo> ex) {
    printf("%s: %s\n", label, ex.err);
}

# the case of the issue
sub issue() {
    list<auto> src = ((1,), (2,));
    list<auto> wl;
    wl := src;
    try { printf("remove wl[-5]: %y\n", remove wl[-5]); } catch (hash<ExceptionInfo> ex) { err("remove wl[-5]", ex); }
    int k = -5;
    try { printf("remove wl[k]: %y\n", remove wl[k]); } catch (hash<ExceptionInfo> ex) { err("remove wl[k]", ex); }
    try { delete wl[-5]; } catch (hash<ExceptionInfo> ex) { err("delete wl[-5]", ex); }
    try { printf("remove src[-5]: %y\n", remove src[-5]); } catch (hash<ExceptionInfo> ex) { err("remove src[-5]", ex); }
    printf("issue: src %y wl %y\n", src, wl);
}

sub list_local() {
    list<auto!> src = (1, 2, 3, 4);
    list<auto!> wl;
    wl := src;
    printf("remove wl[1]: %y\n", remove wl[1]);
    printf("  src %y wl %y\n", src, wl);
    printf("remove wl[-1]: %y\n", remove wl[-1]);
    printf("  src %y\n", src);
    printf("remove wl[9]: %y\n", remove wl[9]);
    printf("  src %y\n", src);
    int i = 0;
    printf("remove wl[i]: %y\n", remove wl[i]);
    printf("  src %y\n", src);

    src = (1, 2, 3, 4, 5);
    wl := src;
    printf("remove wl[1, 3]: %y\n", remove wl[1, 3]);
    printf("  src %y\n", src);
    list<int> idx = (0, 1);
    printf("remove wl[idx]: %y\n", remove wl[idx]);
    printf("  src %y\n", src);

    src = (1, 2, 3, 4, 5);
    wl := src;
    printf("remove wl[1..2]: %y\n", remove wl[1..2]);
    printf("  src %y\n", src);
    printf("remove wl[5..6]: %y\n", remove wl[5..6]);
    printf("  src %y\n", src);

    src = (1, 2, 3);
    wl := src;
    delete wl[0];
    printf("delete wl[0]: src %y\n", src);
    delete wl[1, 2];
    printf("delete wl[1, 2]: src %y\n", src);
}

sub list_holders() {
    # a closure that captures the variable holding the weak reference
    list<auto!> src = (1, 2, 3);
    list<auto!> wl;
    wl := src;
    code c = auto sub () { return remove wl[0]; };
    printf("closure remove: %y src %y\n", c(), src);

    # a member, a global and a hash key holding a weak reference
    M m();
    src = (1, 2, 3);
    m.wl := src;
    printf("member remove: %y src %y\n", remove m.wl[1], src);
    src = (1, 2, 3);
    gwl := src;
    printf("global remove: %y src %y\n", remove gwl[2], src);
    hash<auto!> outer = {"l": NOTHING, "x": 1};
    src = (1, 2, 3);
    outer.l := src;
    printf("hash key remove: %y src %y\n", remove outer.l[0], src);
    printf("hash key slice remove: %y src %y\n", remove outer.l[1, 2], src);
}

sub list_typed() {
    # a list whose element type does not take NOTHING keeps its type through a weak reference, which its owners share
    list<int> src = (1, 2, 3);
    list<int> wl;
    wl := src;
    try {
        remove wl[0];
    } catch (hash<ExceptionInfo> ex) {
        printf("typed remove: %s: %s\n", ex.err, ex.desc);
    }
    try {
        delete wl[0];
    } catch (hash<ExceptionInfo> ex) {
        printf("typed delete: %s: %s\n", ex.err, ex.desc);
    }
    printf("  src %y type %s\n", src, src.fullType());
    # a slice and a range take the elements out of the list
    printf("typed slice: %y src %y type %s\n", remove wl[0, 2], src, src.fullType());

    list<*int> osrc = (1, 2, 3);
    list<*int> owl;
    owl := osrc;
    printf("or-nothing remove: %y src %y\n", remove owl[0], osrc);
}

sub hash_local() {
    hash<auto!> src = {"a": 1, "b": 2, "c": 3, "d": 4};
    hash<auto!> wh;
    wh := src;
    printf("remove wh.a: %y\n", remove wh.a);
    printf("  src %y wh %y\n", src, wh);
    string k = "b";
    printf("remove wh{k}: %y\n", remove wh{k});
    printf("remove wh.x: %y\n", remove wh.x);
    printf("  src %y\n", src);
    src = {"a": 1, "b": 2, "c": 3, "d": 4};
    wh := src;
    printf("remove wh{\"a\", \"b\"}: %y\n", remove wh{"a", "b"});
    printf("remove wh.(\"c\", \"x\"): %y\n", remove wh.("c", "x"));
    printf("  src %y\n", src);
    src = {"a": 1, "b": 2};
    wh := src;
    delete wh.a;
    printf("delete wh.a: src %y\n", src);

    M m();
    src = {"a": 1, "b": 2};
    m.wh := src;
    printf("member key remove: %y src %y\n", remove m.wh.b, src);
}

sub freed() {
    list<auto!> wl;
    {
        list<auto!> src = (1, 2, 3);
        wl := src;
    }
    try { remove wl[0]; } catch (hash<ExceptionInfo> ex) { err("freed remove wl[0]", ex); }
    try { remove wl[-5]; } catch (hash<ExceptionInfo> ex) { err("freed remove wl[-5]", ex); }
    try { delete wl[0]; } catch (hash<ExceptionInfo> ex) { err("freed delete wl[0]", ex); }
    try { remove wl[0, 1]; } catch (hash<ExceptionInfo> ex) { err("freed remove wl[0, 1]", ex); }
    try { remove wl[0..1]; } catch (hash<ExceptionInfo> ex) { err("freed remove wl[0..1]", ex); }
    # a copy of a list freed under a weak reference keeps its freed state (qore#7163)
    list<auto!> c = wl;
    try { remove c[0]; } catch (hash<ExceptionInfo> ex) { err("freed copy remove c[0]", ex); }
    try { delete c[5]; } catch (hash<ExceptionInfo> ex) { err("freed copy delete c[5]", ex); }
    try { remove c[0, 1]; } catch (hash<ExceptionInfo> ex) { err("freed copy remove c[0, 1]", ex); }
    try { remove c[0..1]; } catch (hash<ExceptionInfo> ex) { err("freed copy remove c[0..1]", ex); }
    try { remove c[-1]; } catch (hash<ExceptionInfo> ex) { err("freed copy remove c[-1]", ex); }
    try { delete c[-1]; } catch (hash<ExceptionInfo> ex) { err("freed copy delete c[-1]", ex); }
    int ni = -1;
    try { remove c[ni]; } catch (hash<ExceptionInfo> ex) { err("freed copy remove c[ni]", ex); }
    hash<auto!> wh;
    {
        hash<auto!> h = {"a": 1};
        wh := h;
    }
    try { remove wh.a; } catch (hash<ExceptionInfo> ex) { err("freed remove wh.a", ex); }
    try { delete wh.a; } catch (hash<ExceptionInfo> ex) { err("freed delete wh.a", ex); }
    try { remove wh{"a", "b"}; } catch (hash<ExceptionInfo> ex) { err("freed remove wh{}", ex); }
    hash<auto!> hc = wh;
    try { remove hc.a; } catch (hash<ExceptionInfo> ex) { err("freed copy remove hc.a", ex); }
    try { remove hc{"a", "b"}; } catch (hash<ExceptionInfo> ex) { err("freed copy remove hc{}", ex); }
}

sub objects() {
    list<auto!> src = (new D("l0"), new D("l1"), 3);
    list<auto!> wl;
    wl := src;
    delete wl[0];
    auto r = remove wl[1];
    printf("removed %s; src %y\n", r.n, src[2]);
    delete r;
    list<auto!> s2 = (new D("r0"), new D("r1"), new D("r2"));
    list<auto!> w2;
    w2 := s2;
    delete w2[0..1];
    printf("after range delete: %d\n", s2.size());
    hash<auto!> h = {"a": new D("ha"), "b": new D("hb"), "c": 1};
    hash<auto!> wh;
    wh := h;
    delete wh.a;
    delete wh{"b"};
    printf("after key deletes: %y\n", keys h);

    # an object removed through the weak reference of a container that an object holds (see qore#7318)
    D c("c");
    c.holder = {"inner": new list<auto!>((new D("x"), 1))};
    auto w;
    w := c.holder.inner;
    remove w[0];
    remove c.holder.inner;
    printf("holder: %y\n", c.holder);

    # a cycle broken through the weak reference
    D c3("c3");
    c3.holder = {"inner": new list<auto!>((c3,))};
    auto w3;
    w3 := c3.holder.inner;
    remove w3[0];
    printf("cycle broken\n");
}

# changes made without a weak reference are unchanged
sub plain() {
    list<auto> l = (1, 2, 3);
    printf("plain remove: %y l %y\n", remove l[0], l);
    try { remove l[-5]; } catch (hash<ExceptionInfo> ex) { err("plain remove l[-5]", ex); }
    hash<auto> h = {"a": 1, "b": 2};
    printf("plain key remove: %y h %y\n", remove h.a, h);
    list<auto> l2 = (1, 2, 3, 4);
    printf("plain range remove: %y l2 %y\n", remove l2[1..2], l2);
}

sub references() {
    list<auto!> src = (1, 2, 3);
    list<auto!> wl;
    wl := src;
    reference<auto> r = \wl;
    printf("reference remove: %y src %y\n", remove r[0], src);
    reference<auto> er = \wl[1];
    remove er;
    printf("element reference remove: src %y\n", src);
    hash<auto!> hsrc = {"a": 1, "b": 2};
    hash<auto!> wh;
    wh := hsrc;
    reference<auto> hr = \wh;
    printf("hash reference remove: %y hsrc %y\n", remove hr.a, hsrc);
}
sub statics() {
    list<auto!> src = (1, 2, 3);
    S::swl := src;
    printf("static remove: %y src %y\n", remove S::swl[0], src);
    hash<auto!> hsrc = {"a": 1, "b": 2};
    S::swh := hsrc;
    printf("static key remove: %y hsrc %y\n", remove S::swh.b, hsrc);
}
sub nested() {
    list<auto!> nsrc = ((1, 2, 3), (4, 5, 6));
    list<auto!> nw;
    nw := nsrc;
    printf("nested remove: %y\n", remove nw[0][1]);
    printf("nested range remove: %y\n", remove nw[1][0..0]);
    printf("nested slice remove: %y\n", remove nw[1][0, 1]);
    printf("  nsrc %y\n", nsrc);
    hash<auto!> hsrc = {"a": {"b": 1, "c": 2, "d": 3}, "l": (1, 2, 3)};
    hash<auto!> nhw;
    nhw := hsrc;
    printf("nested key remove: %y\n", remove nhw.a.b);
    printf("nested hash range remove: %y\n", remove nhw.l[0..0]);
    printf("nested key slice remove: %y\n", remove nhw.a{"c", "d"});
    printf("  hsrc %y\n", hsrc);
}
sub element_weak() {
    list<auto!> src = (1, 2, 3, 4, 5);
    list<auto!> outer = (NOTHING,);
    outer[0] := src;
    printf("element weak remove: %y\n", remove outer[0][0]);
    printf("element weak range remove: %y\n", remove outer[0][1..2]);
    printf("element weak slice remove: %y\n", remove outer[0][0, 1]);
    delete outer[0][0];
    printf("  src %y outer %y\n", src, outer);
}
sub typed_hashes() {
    hash<string, int> tsrc = {"a": 1, "b": 2};
    hash<string, int> twh;
    twh := tsrc;
    printf("typed hash remove: %y tsrc %y type %s\n", remove twh.a, tsrc, tsrc.fullType());
    hash<HD> dsrc();
    hash<HD> dwh;
    dwh := dsrc;
    try { printf("hashdecl remove: %y\n", remove dwh.b); } catch (hash<ExceptionInfo> ex) { err("hashdecl remove", ex); }
    try { printf("hashdecl required remove: %y\n", remove dwh.a); } catch (hash<ExceptionInfo> ex) { err("hashdecl required remove", ex); }
    printf("  dsrc %y\n", dsrc);
    hash<HD> plain();
    try { printf("plain hashdecl remove: %y\n", remove plain.b); } catch (hash<ExceptionInfo> ex) { err("plain hashdecl remove", ex); }
    try { printf("plain hashdecl required remove: %y\n", remove plain.a); } catch (hash<ExceptionInfo> ex) { err("plain hashdecl required remove", ex); }
    printf("  plain %y\n", plain);
}
sub typed_negative() {
    list<int> src = (1, 2, 3);
    list<int> wl;
    wl := src;
    try { remove wl[-1]; } catch (hash<ExceptionInfo> ex) { err("typed remove wl[-1]", ex); }
    try { remove wl[-5]; } catch (hash<ExceptionInfo> ex) { err("typed remove wl[-5]", ex); }
    printf("  src %y\n", src);
}

issue();
list_local();
list_holders();
list_typed();
hash_local();
freed();
objects();
plain();
references();
statics();
nested();
element_weak();
typed_hashes();
typed_negative();
printf("end\n");
