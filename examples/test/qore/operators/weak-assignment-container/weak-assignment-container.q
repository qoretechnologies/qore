#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# a weak assignment of a typed list or hash designates the container assigned: an lvalue whose type keeps the
# container takes the weak reference to it, and one whose type would convert it to a copy, also one that only strips
# the container's type, refuses the weak assignment and keeps its value (qore#7237, qore#7572)

%modern
%allow-weak-references
%allow-opaque-references

hashdecl Decl {
    int a;
}

hashdecl ListHolder {
    *list l;
}

class Holder {
    public {
        list l = (0,);
        hash h = {"z": 0};
        any a = 0;
        *softlist sl;
    }
}

our list gl = (0,);
our hash gh = {"z": 0};
thread_local list tl = (0,);

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
    return ex.err == "RUNTIME-TYPE-ERROR" && ex.desc =~ /assignment operator '(:|@)=' cannot store a reference/
        ? "refused (weak assignment)"
        : sprintf("%s: %s", ex.err, ex.desc);
}

#! weak assignments to variables; the values are typed at run time only, as the parser refuses the weak assignments
#! to a stripping lvalue that it can see (qore#7572)
sub variables() {
    auto src = cast<list<int>>((1, 2));
    auto hsrc = cast<hash<string, int>>({"a": 1});

    list l = (0,);
    try {
        l := src;
        printf("list variable: %s\n", show(l));
    } catch (hash<ExceptionInfo> ex) {
        printf("list variable: %s; kept %s\n", refusal(ex), show(l));
    }
    hash h = {"z": 0};
    try {
        h := hsrc;
        printf("hash variable: %s\n", show(h));
    } catch (hash<ExceptionInfo> ex) {
        printf("hash variable: %s; kept %s\n", refusal(ex), show(h));
    }
    any a = 0;
    try {
        a := src;
        printf("any variable: %s\n", show(a));
    } catch (hash<ExceptionInfo> ex) {
        printf("any variable: %s; kept %s\n", refusal(ex), show(a));
    }
    softlist sl = (0,);
    try {
        sl := src;
        printf("softlist variable: %s\n", show(sl));
    } catch (hash<ExceptionInfo> ex) {
        printf("softlist variable: %s; kept %s\n", refusal(ex), show(sl));
    }
    *list ol = (0,);
    try {
        ol := src;
        printf("*list variable: %s\n", show(ol));
    } catch (hash<ExceptionInfo> ex) {
        printf("*list variable: %s; kept %s\n", refusal(ex), show(ol));
    }
    *hash oh = {"z": 0};
    try {
        oh := hsrc;
        printf("*hash variable: %s\n", show(oh));
    } catch (hash<ExceptionInfo> ex) {
        printf("*hash variable: %s; kept %s\n", refusal(ex), show(oh));
    }
    list<auto!> nl = (0,);
    try {
        nl := src;
        printf("list<auto!> variable: %s\n", show(nl));
    } catch (hash<ExceptionInfo> ex) {
        printf("list<auto!> variable: %s; kept %s\n", refusal(ex), show(nl));
    }
    hash<auto!> nh = {"z": 0};
    try {
        nh := hsrc;
        printf("hash<auto!> variable: %s\n", show(nh));
    } catch (hash<ExceptionInfo> ex) {
        printf("hash<auto!> variable: %s; kept %s\n", refusal(ex), show(nh));
    }
    auto! na = 0;
    try {
        na := src;
        printf("auto! variable: %s\n", show(na));
    } catch (hash<ExceptionInfo> ex) {
        printf("auto! variable: %s; kept %s\n", refusal(ex), show(na));
    }

    # a closure-bound variable
    list cl = (0,);
    code c = auto sub () { return cl; };
    try {
        cl := src;
        printf("closure list variable: %s\n", show(c()));
    } catch (hash<ExceptionInfo> ex) {
        printf("closure list variable: %s; kept %s\n", refusal(ex), show(c()));
    }

    # global and thread-local variables
    try {
        gl := src;
        printf("global list: %s\n", show(gl));
    } catch (hash<ExceptionInfo> ex) {
        printf("global list: %s; kept %s\n", refusal(ex), show(gl));
    }
    try {
        gh := hsrc;
        printf("global hash: %s\n", show(gh));
    } catch (hash<ExceptionInfo> ex) {
        printf("global hash: %s; kept %s\n", refusal(ex), show(gh));
    }
    try {
        tl := src;
        printf("thread_local list: %s\n", show(tl));
    } catch (hash<ExceptionInfo> ex) {
        printf("thread_local list: %s; kept %s\n", refusal(ex), show(tl));
    }

    # lvalues whose types keep the containers: the weak references designate the containers assigned
    list<int> same;
    same := src;
    list<auto> la;
    la := src;
    hash<auto> ha;
    ha := hsrc;
    auto au;
    au := hsrc;
    list<auto> cla;
    code c2 = auto sub () { return cla; };
    cla := src;
    printf("kept: list<int> %s list<auto> %s hash<auto> %s auto %s closure %s\n", show(same), show(la), show(ha),
        show(au), show(c2()));

    # a change of a container is read through them
    src += 3;
    hsrc.b = 2;
    printf("changed: list<int> %y list<auto> %y hash<auto> %y auto %y closure %y\n", same, la, ha, au, c2());

    # the weak references do not keep the containers assigned
    remove src;
    remove hsrc;
    printf("released: list<int> %y list<auto> %y hash<auto> %y auto %y closure %y\n", same, la, ha, au, c2());
}

#! weak assignments to members of containers and objects
sub container_members() {
    auto src = cast<list<int>>((1, 2));
    auto hsrc = cast<hash<string, int>>({"a": 1});

    # an element of an untyped list (the case of qore#7237)
    list holder = ();
    holder += 1;
    try {
        holder[0] := src;
        printf("untyped list element: %s\n", show(holder[0]));
    } catch (hash<ExceptionInfo> ex) {
        printf("untyped list element: %s; kept %y\n", refusal(ex), holder);
    }
    # an element of a list of lists
    list<list> ll = ();
    ll += (list(),);
    try {
        ll[0] := src;
        printf("list<list> element: %s\n", show(ll[0]));
    } catch (hash<ExceptionInfo> ex) {
        printf("list<list> element: %s; kept %y\n", refusal(ex), ll);
    }
    # a member of an untyped hash
    hash hh = {};
    try {
        hh.a := src;
        printf("untyped hash member: %s\n", show(hh.a));
    } catch (hash<ExceptionInfo> ex) {
        printf("untyped hash member: %s; kept %y\n", refusal(ex), hh);
    }
    # a member of a hash of lists
    hash<string, list> hl = {};
    try {
        hl.a := src;
        printf("hash<string, list> member: %s\n", show(hl.a));
    } catch (hash<ExceptionInfo> ex) {
        printf("hash<string, list> member: %s; kept %y\n", refusal(ex), hl);
    }
    # a key of a hashdecl
    hash<ListHolder> lh();
    try {
        lh.l := src;
        printf("hashdecl key: %s\n", show(lh.l));
    } catch (hash<ExceptionInfo> ex) {
        printf("hashdecl key: %s; kept %y\n", refusal(ex), lh);
    }
    # members of an object
    Holder o();
    try {
        o.l := src;
        printf("object list member: %s\n", show(o.l));
    } catch (hash<ExceptionInfo> ex) {
        printf("object list member: %s; kept %y\n", refusal(ex), o.l);
    }
    try {
        o.h := hsrc;
        printf("object hash member: %s\n", show(o.h));
    } catch (hash<ExceptionInfo> ex) {
        printf("object hash member: %s; kept %y\n", refusal(ex), o.h);
    }
    try {
        o.sl := src;
        printf("object softlist member: %s\n", show(o.sl));
    } catch (hash<ExceptionInfo> ex) {
        printf("object softlist member: %s; kept %y\n", refusal(ex), o.sl);
    }
    # an opaque assignment is refused as a weak one is
    try {
        o.a @= src;
        printf("object any member (opaque): %s\n", show(o.a));
    } catch (hash<ExceptionInfo> ex) {
        printf("object any member (opaque): %s; kept %y\n", refusal(ex), o.a);
    }
    # a container whose elements have a complex type: the strip of an untyped lvalue would copy it deeply
    auto nested = cast<list<list<int>>>(((1, 2),));
    list nl = ();
    nl += 1;
    try {
        nl[0] := nested;
        printf("nested: %s\n", show(nl[0]));
    } catch (hash<ExceptionInfo> ex) {
        printf("nested: %s; kept %y\n", refusal(ex), nl);
    }
    # a hashdecl value: the type of a hash or hash<auto!> lvalue strips its hashdecl
    auto dsrc = <Decl>{"a": 5};
    hash dh = {"z": 0};
    try {
        dh := dsrc;
        printf("hashdecl to hash: %s\n", show(dh));
    } catch (hash<ExceptionInfo> ex) {
        printf("hashdecl to hash: %s; kept %y\n", refusal(ex), dh);
    }
    hash<auto!> dah = {"z": 0};
    try {
        dah := dsrc;
        printf("hashdecl to hash<auto!>: %s\n", show(dah));
    } catch (hash<ExceptionInfo> ex) {
        printf("hashdecl to hash<auto!>: %s; kept %y\n", refusal(ex), dah);
    }
    # a weak reference assigned weakly: the container that it designates is checked
    auto wl;
    wl := src;
    list wl2 = (0,);
    try {
        wl2 := wl;
        printf("weak source variable: %s\n", show(wl2));
    } catch (hash<ExceptionInfo> ex) {
        printf("weak source variable: %s; kept %y\n", refusal(ex), wl2);
    }
    list wholder = ();
    wholder += 1;
    try {
        wholder[0] := wl;
        printf("weak source element: %s\n", show(wholder[0]));
    } catch (hash<ExceptionInfo> ex) {
        printf("weak source element: %s; kept %y\n", refusal(ex), wholder);
    }

    # lvalues whose types keep the containers: the weak references designate the containers assigned
    list<auto> kl = ();
    kl += 1;
    kl[0] := src;
    hash<string, auto> kh = {};
    kh.a := hsrc;
    list<auto> kw = ();
    kw += 1;
    kw[0] := wl;
    hash<Decl> kd;
    kd := dsrc;
    printf("kept: list<auto> element %s hash<string, auto> member %s weak source %s hashdecl %s\n", show(kl[0]),
        show(kh.a), show(kw[0]), show(kd));
    # the overload selected for the value of an element
    auto ls = cast<list<string>>(("a",));
    list<auto> ovl = ();
    ovl += 1;
    ovl[0] := ls;
    printf("overload: %s\n", ov(ovl[0]));

    # a change of a container is read through them
    src += 3;
    hsrc.c = 3;
    dsrc.a = 6;
    printf("changed: %y %y %y %y\n", kl[0], kh.a, kw[0], kd);
    # an element written through the weak reference of a list element changes the container assigned
    kl[0][3] = 4;
    printf("written through the element: %y\n", src);

    remove src;
    remove hsrc;
    remove dsrc;
    printf("released: %y %y %y %y\n", kl[0], kh.a, kw[0], kd);
}

#! types that convert the container to another: the weak assignment is refused, and the lvalue keeps its value
sub conversions() {
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
    auto strs = cast<list<string>>(("1", "2"));
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
    auto shash = cast<hash<string, string>>({"a": "1"});
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
        printf("list to hash: %s %s\n", ex.err, ex.desc =~ /cannot store a reference/ ? "refused" : "rejected");
    }
}

variables();
container_members();
conversions();
