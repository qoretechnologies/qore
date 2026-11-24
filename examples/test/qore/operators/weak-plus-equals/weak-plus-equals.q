#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# += on an lvalue that holds a weak reference to a list or a hash changes the container that the weak reference
# designates, and the lvalue keeps the weak reference (qore#7297)

%modern
%allow-weak-references

hashdecl Decl {
    int a;
    *int b;
}

our list<auto> gw;
thread_local list<auto> tw;

class Holder {
    public {
        list<auto> l;
        hash<auto> h;
    }
}

class Members {
    public {
        int x = 1;
        string y = "y";
    }
}

our list<string> destroyed = ();

#! records its destruction
class Tracked {
    public {
        string name;
        hash<auto> holder;
    }

    constructor(string name) {
        self.name = name;
    }

    destructor() {
        destroyed += name;
    }
}

#! returns its argument, whose type the parser does not know
auto sub untyped(auto v) {
    return v;
}

#! describes an exception
string sub describe(hash<ExceptionInfo> ex) {
    return ex.err;
}

sub lists() {
    list<int> src = (1, 2);

    auto a;
    a := src;
    a += 3;
    a += (4, 5);
    printf("auto variable: src %y lvalue %y (%s)\n", src, a, a.fullType());
    list<int> t;
    t := src;
    t += 6;
    list<auto> la;
    la := src;
    la += 7;
    printf("typed variables: %y\n", src);
    # the value of the operation is the container changed
    auto v = (la += 8);
    printf("value: %y (%s)\n", v, v.fullType());
    remove v;
    # the container's own type checks the value added
    try {
        la += untyped("x");
        printf("string added: %y\n", src);
    } catch (hash<ExceptionInfo> ex) {
        printf("string added: %s; src %y\n", describe(ex), src);
    }

    # a closure-bound, global and thread-local variable, an object member, a hash key and a list element
    list<auto> cw;
    code c = auto sub () { return cw; };
    cw := src;
    cw += 9;
    gw := src;
    gw += 10;
    tw := src;
    tw += 11;
    Holder o();
    o.l := src;
    o.l += 12;
    hash<auto> h = {};
    h.k := src;
    h.k += 13;
    list<auto!> l = (NOTHING,);
    l[0] := src;
    l[0] += 14;
    printf("lvalues: %y\n", src);
    printf("read through them: %y %y %y %y %y %y\n", c(), gw, tw, o.l, h.k, l[0]);

    # the list added to itself, through the weak reference or as the operand
    a += src;
    printf("added to itself: %d elements\n", src.size());
    a += a;
    printf("added to itself twice: %d elements, %y\n", src.size(), src[0..3]);
    # no value: the list's element type refuses it, as for the list itself
    try {
        a += NOTHING;
        printf("nothing added: %d elements\n", src.size());
    } catch (hash<ExceptionInfo> ex) {
        printf("nothing added: %s; %d elements\n", describe(ex), src.size());
    }

    # the lvalue is an entry of the list that its weak reference designates: the list grows under it
    list<auto!> own = (NOTHING,);
    auto ow;
    ow := own;
    # assigned through the weak reference: the entry is in the list itself
    ow[0] := own;
    for (int i = 0; i < 100; ++i) {
        own[0] += i;
    }
    printf("own entry: %d elements, last %y\n", own.size(), own[100]);
    remove ow;
    remove own;

    # the lvalues keep the weak reference: the container is released with its last strong reference
    remove src;
    printf("released: %y %y %y %y %y %y %y %y\n", a, t, la, cw, gw, o.l, h.k, l[0]);

    # a freed container cannot be changed
    try {
        a += 1;
        printf("freed: %y\n", a);
    } catch (hash<ExceptionInfo> ex) {
        printf("freed: %s\n", describe(ex));
    }
    try {
        t += 1;
        printf("freed typed: %y\n", t);
    } catch (hash<ExceptionInfo> ex) {
        printf("freed typed: %s\n", describe(ex));
    }
}

sub hashes() {
    hash<string, int> src = {"a": 1};

    auto a;
    a := src;
    a += {"b": 2};
    hash<string, int> t;
    t := src;
    t += {"c": 3};
    Holder o();
    o.h := src;
    o.h += {"d": 4};
    printf("hash: %y\n", src);
    auto v = (a += {"e": 5});
    printf("value: %y (%s)\n", v, v.fullType());
    remove v;

    # the hash added to itself, and an object merged into a hash
    a += src;
    printf("added to itself: %y\n", src);
    hash<auto!> any_src = {"a": 1};
    auto aw;
    aw := any_src;
    aw += new Members();
    printf("object merged: %y\n", any_src);

    hash<Decl> d = <Decl>{"a": 1};
    hash<Decl> dw;
    dw := d;
    dw += {"b": 2};
    printf("hashdecl: %y (%s)\n", d, d.fullType());
    auto da;
    da := d;
    da += {"b": 3};
    printf("hashdecl through an auto lvalue: %y (%s)\n", d, d.fullType());
    try {
        dw += untyped({"x": 1});
        printf("hashdecl unknown key: %y\n", d);
    } catch (hash<ExceptionInfo> ex) {
        printf("hashdecl unknown key: %s; %y\n", describe(ex), d);
    }

    remove src;
    printf("released: %y %y %y\n", a, t, o.h);
    try {
        t += {"z": 1};
        printf("freed: %y\n", t);
    } catch (hash<ExceptionInfo> ex) {
        printf("freed: %s\n", describe(ex));
    }
    try {
        a += {"z": 1};
        printf("freed auto: %y\n", a);
    } catch (hash<ExceptionInfo> ex) {
        printf("freed auto: %s\n", describe(ex));
    }
}

#! the lvalue is an entry of the list that it changes, which an object's member holds: the object counts of the
#! containers on the lvalue's path are updated, and the objects are destroyed when they are released
sub object_counts() {
    {
        Tracked c("holder");
        # the entry of the list is a weak reference to the list itself, which the object's member then holds alone
        list<auto!> inner = (NOTHING,);
        auto w;
        w := inner;
        w[0] := inner;
        remove w;
        c.holder = {"inner": inner};
        remove inner;
        c.holder.inner[0] += new Tracked("added");
        printf("added through the entry: %d elements\n", c.holder.inner.size());
        remove c.holder.inner;
    }
    printf("destroyed: %y\n", destroyed);
    # a cycle made by adding the object to the list that its member holds: it is collected
    {
        Tracked c("cycle");
        list<auto!> inner = (NOTHING,);
        auto w;
        w := inner;
        w[0] := inner;
        remove w;
        c.holder = {"inner": inner};
        remove inner;
        c.holder.inner[0] += c;
    }
    printf("cycle destroyed: %y\n", destroyed);
}

lists();
hashes();
object_counts();
