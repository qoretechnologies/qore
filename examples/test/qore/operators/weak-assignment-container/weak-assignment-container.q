#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# a weak assignment of a typed list or hash designates the container assigned, also where the lvalue's type strips
# the container's type; a type that converts the container to another refuses the weak assignment (qore#7237)

%modern
%allow-weak-references

hashdecl Decl {
    int a;
}

hashdecl ListHolder {
    *list l;
}

class Holder {
    public {
        list l;
        hash h;
        any a;
        *softlist sl;
    }
}

our list gl;
our hash gh;
thread_local list tl;

string sub ov(list<list<int>> l) {
    return "list<list<int>>";
}

string sub ov(list<string> l) {
    return "list<string>";
}

#! describes the value \a v and its type
string sub show(auto v) {
    return sprintf("%y (%s)", v, v.fullType());
}

#! describes an exception: the refusal of a weak assignment, or another exception
string sub refusal(hash<ExceptionInfo> ex) {
    return ex.err == "RUNTIME-TYPE-ERROR" && ex.desc =~ /a weak assignment \(:=\)/
        ? "refused (weak assignment)"
        : sprintf("%s: %s", ex.err, ex.desc);
}

sub variables() {
    list<int> src = (1, 2);
    hash<string, int> hsrc = {"a": 1};

    list l;
    l := src;
    printf("list variable: %s\n", show(l));
    hash h;
    h := hsrc;
    printf("hash variable: %s\n", show(h));
    any a;
    a := src;
    printf("any variable: %s\n", show(a));
    softlist sl;
    sl := src;
    printf("softlist variable: %s\n", show(sl));
    *list ol;
    ol := src;
    printf("*list variable: %s\n", show(ol));
    *hash oh;
    oh := hsrc;
    printf("*hash variable: %s\n", show(oh));
    list<auto!> nl;
    nl := src;
    printf("list<auto!> variable: %s\n", show(nl));
    hash<auto!> nh;
    nh := hsrc;
    printf("hash<auto!> variable: %s\n", show(nh));
    auto! na;
    na := src;
    printf("auto! variable: %s\n", show(na));
    list<int> same;
    same := src;
    printf("list<int> variable: %s\n", show(same));

    # a closure-bound variable
    list cl;
    code c = auto sub () { return cl; };
    cl := src;
    printf("closure list variable: %s\n", show(c()));

    # global and thread-local variables
    gl := src;
    gh := hsrc;
    tl := src;
    printf("global list: %s hash: %s thread_local list: %s\n", show(gl), show(gh), show(tl));

    # the weak references designate the containers assigned: a change of a container is read through them
    src += 3;
    hsrc.b = 2;
    printf("changed: list %y hash %y any %y softlist %y *list %y *hash %y list<auto!> %y hash<auto!> %y auto! %y "
        "closure %y global %y %y thread_local %y\n", l, h, a, sl, ol, oh, nl, nh, na, c(), gl, gh, tl);

    # the weak references do not keep the containers assigned
    remove src;
    remove hsrc;
    printf("released: list %y hash %y closure %y global %y\n", l, h, c(), gl);
}

sub container_members() {
    list<int> src = (1, 2);
    hash<string, int> hsrc = {"a": 1};

    # an element of an untyped list (the issue's case)
    list holder = ();
    holder += 1;
    holder[0] := src;
    printf("untyped list element: %s\n", show(holder[0]));
    # an element of a list of lists
    list<list> ll = ();
    ll += (list(),);
    ll[0] := src;
    printf("list<list> element: %s\n", show(ll[0]));
    # a member of an untyped hash
    hash hh = {};
    hh.a := src;
    hh.b := hsrc;
    printf("untyped hash members: %s %s\n", show(hh.a), show(hh.b));
    # a member of a hash of lists and of a hash of hashes
    hash<string, list> hl = {};
    hl.a := src;
    hash<string, hash> hhh = {};
    hhh.a := hsrc;
    printf("hash<string, list> member: %s hash<string, hash> member: %s\n", show(hl.a), show(hhh.a));
    # a key of a hashdecl
    hash<ListHolder> lh();
    lh.l := src;
    printf("hashdecl key: %s\n", show(lh.l));
    # members of an object
    Holder o();
    o.l := src;
    o.h := hsrc;
    o.a := src;
    o.sl := src;
    printf("object members: %s %s %s %s\n", show(o.l), show(o.h), show(o.a), show(o.sl));
    # a container whose elements have a complex type: the strip of an untyped lvalue would copy it deeply
    list<list<int>> nested = ((1, 2),);
    list nh = ();
    nh += 1;
    nh[0] := nested;
    printf("nested: %s\n", show(nh[0]));

    # a hashdecl value: the type of a hash or hash<auto!> lvalue strips its hashdecl, and the weak reference
    # designates it with its hashdecl
    hash<Decl> dsrc = <Decl>{"a": 5};
    hash dh;
    dh := dsrc;
    hash<auto!> dah;
    dah := dsrc;
    printf("hashdecl kept: %s %s\n", show(dh), show(dah));
    # a weak reference assigned weakly designates the container that it designates
    list wl;
    wl := src;
    list wl2;
    wl2 := wl;
    list wholder = ();
    wholder += 1;
    wholder[0] := wl;
    printf("weak source: %s %s\n", show(wl2), show(wholder[0]));

    # the overload selected for the value of an element
    list<string> ls = ("a",);
    list ol = ();
    ol += 1;
    ol[0] := ls;
    printf("overload: %s\n", ov(ol[0]));

    # the weak references designate the containers assigned: a change of a container is read through them
    src += 3;
    hsrc.c = 3;
    nested[1] = (9,);
    dsrc.a = 6;
    printf("changed: %y %y %y %y %y %y %y %y %y %y %y %y %y %y\n", holder[0], ll[0], hh.a, hh.b, hl.a, hhh.a, lh.l, o.l,
        o.h, nh[0], dh, dah, wl2, wholder[0]);
    # an element written through the weak reference of a list element changes the container assigned
    holder[0][3] = 4;
    printf("written through the element: %y\n", src);

    remove src;
    remove hsrc;
    remove ls;
    remove nested;
    remove dsrc;
    printf("released: %y %y %y %y %y %y %y %y %y %y %y\n", holder[0], ll[0], hh.a, hh.b, hl.a, lh.l, o.l, nh[0], dh,
        wl2, wholder[0]);
}

sub conversions() {
    # a type that converts the container to another: the weak assignment is refused, and the lvalue keeps its value
    # the values are typed at run time only, as the parser refuses the assignments that it can see
    auto hsrc = cast<hash<string, int>>({"a": 1});
    # a plain hash, which the hashdecl type rejects, as for a normal assignment: the lvalue keeps its value
    hash plain = {"a": 1};
    auto psrc = plain;
    hash<Decl> hd = <Decl>{"a": 0};
    try {
        hd := psrc;
        printf("hashdecl: %y\n", hd);
    } catch (hash<ExceptionInfo> ex) {
        printf("hashdecl: %s; kept %y\n", refusal(ex), hd);
    }
    list<string> strs = ("1", "2");
    list<softint> si = (0,);
    try {
        si := strs;
        printf("list<softint>: %y\n", si);
    } catch (hash<ExceptionInfo> ex) {
        printf("list<softint>: %s; kept %y\n", refusal(ex), si);
    }
    list<softint> holder = (0,);
    list<list<softint>> lsi = (holder,);
    try {
        lsi[0] := strs;
        printf("list<list<softint>> element: %y\n", lsi);
    } catch (hash<ExceptionInfo> ex) {
        printf("list<list<softint>> element: %s; kept %y\n", refusal(ex), lsi);
    }
    hash<string, hash<string, softint>> hsi = {};
    hash<string, string> shash = {"a": "1"};
    try {
        hsi.a := shash;
        printf("hash<string, hash<string, softint>> member: %y\n", hsi);
    } catch (hash<ExceptionInfo> ex) {
        printf("hash<string, hash<string, softint>> member: %s; kept %y\n", refusal(ex), hsi);
    }
    # a softlist lvalue given a hash: the type makes a list of it, which only the weak reference would hold
    try {
        softlist sl = (0,);
        sl := hsrc;
        printf("softlist from a hash: %y\n", sl);
    } catch (hash<ExceptionInfo> ex) {
        printf("softlist from a hash: %s\n", refusal(ex));
    }
    # a value of another kind is refused by the type, as by a normal assignment
    auto src = cast<list<int>>((1,));
    try {
        hash h;
        h := src;
        printf("list to hash: %y\n", h);
    } catch (hash<ExceptionInfo> ex) {
        # a type rejection, not the refusal of the weak assignment
        printf("list to hash: %s %s\n", ex.err, ex.desc =~ /weak assignment/ ? "refused" : "rejected");
    }
}

variables();
container_members();
conversions();
