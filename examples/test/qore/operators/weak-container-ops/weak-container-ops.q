#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# the operators that change the list or the hash that an lvalue holds change the container that a weak reference
# held by the lvalue designates, and the lvalue keeps the weak reference (qore#7297)

%modern
%allow-weak-references

our list<string> destroyed = ();

thread_local auto tw;
thread_local auto th;

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

#! runs an operation and describes its result or the exception that it raises
string sub run(code c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! checks that the lvalue still holds a weak reference, and reads the target through it
string sub through(auto w) {
    return sprintf("%y", w);
}

sub lists() {
    list<auto> src = (1, 2, 3, 4, 5, 6, 7, 8);
    list<auto> w;
    w := src;

    printf("push: %s / %y\n", run(string sub () { push w, 9; return through(w); }), src);
    printf("unshift: %s / %y\n", run(string sub () { unshift w, 0; return through(w); }), src);
    printf("pop: %s / %y\n", run(string sub () { return sprintf("%y", pop w); }), src);
    printf("shift: %s / %y\n", run(string sub () { return sprintf("%y", shift w); }), src);
    printf("splice: %s / %y\n", run(string sub () { splice w, 0, 1; return through(w); }), src);
    printf("splice insert: %s / %y\n", run(string sub () { splice w, 0, 0, (10, 11); return through(w); }), src);
    printf("extract: %s / %y\n", run(string sub () { return sprintf("%y", extract w, 0, 1); }), src);
    printf("remove element: %s / %y\n", run(string sub () { return sprintf("%y", remove w[0]); }), src);
    printf("delete element: %s / %y\n", run(string sub () { delete w[0]; return through(w); }), src);
    printf("remove elements: %s / %y\n", run(string sub () { return sprintf("%y", remove w[1, 2]); }), src);
    printf("remove range: %s / %y\n", run(string sub () { return sprintf("%y", remove w[0..1]); }), src);
    printf("+=: %s / %y\n", run(string sub () { w += (20, 21); return through(w); }), src);

    # the container's own type: an element of a list whose element type refuses NOTHING is not removed
    list<int> ints = (1, 2, 3);
    list<auto> iw;
    iw := ints;
    printf("typed remove element: %s / %y\n", run(string sub () { return sprintf("%y", remove iw[0]); }), ints);
    printf("typed push: %s / %y\n", run(string sub () { push iw, "x"; return through(iw); }), ints);

    # trim and chomp change the strings of the list
    list<auto> strs = (" a ", "b\n");
    list<auto> sw;
    sw := strs;
    trim sw;
    printf("trim: %y\n", strs);
    list<auto> strs2 = ("a\n", "b\n");
    list<auto> sw2;
    sw2 := strs2;
    chomp sw2;
    printf("chomp: %y\n", strs2);

    # the lvalue keeps the weak reference: the container is released with its last strong reference
    remove src;
    printf("released: %y\n", w);
    # a freed list cannot be changed
    foreach string name in (("push", "unshift", "pop", "shift", "splice", "extract", "remove", "delete")) {
        code c;
        switch (name) {
            case "push": c = string sub () { push w, 1; return "changed"; }; break;
            case "unshift": c = string sub () { unshift w, 1; return "changed"; }; break;
            case "pop": c = string sub () { pop w; return "changed"; }; break;
            case "shift": c = string sub () { shift w; return "changed"; }; break;
            case "splice": c = string sub () { splice w, 0, 1; return "changed"; }; break;
            case "extract": c = string sub () { extract w, 0, 1; return "changed"; }; break;
            case "remove": c = string sub () { remove w[0]; return "changed"; }; break;
            case "delete": c = string sub () { delete w[0]; return "changed"; }; break;
        }
        printf("freed %s: %s\n", name, run(c));
    }
}

sub hashes() {
    hash<auto> src = {"a": 1, "b": 2, "c": 3, "d": 4, "e": " x "};
    hash<auto> w;
    w := src;

    printf("-= key: %s / %y\n", run(string sub () { w -= "a"; return through(w); }), src);
    printf("-= list: %s / %y\n", run(string sub () { w -= ("b",); return through(w); }), src);
    printf("delete key: %s / %y\n", run(string sub () { delete w.c; return through(w); }), src);
    printf("remove key: %s / %y\n", run(string sub () { return sprintf("%y", remove w.d); }), src);
    printf("trim: %s / %y\n", run(string sub () { trim w; return through(w); }), src);
    src += {"f": 6, "g": 7};
    printf("remove slice: %s / %y\n", run(string sub () { return sprintf("%y", remove w{"f", "g"}); }), src);

    remove src;
    printf("released: %y\n", w);
    foreach string name in (("-=", "remove", "delete", "slice")) {
        code c;
        switch (name) {
            case "-=": c = string sub () { w -= "a"; return "changed"; }; break;
            case "remove": c = string sub () { remove w.a; return "changed"; }; break;
            case "delete": c = string sub () { delete w.a; return "changed"; }; break;
            case "slice": c = string sub () { remove w{"a",}; return "changed"; }; break;
        }
        printf("freed %s: %s\n", name, run(c));
    }
}

#! a weak reference in a member of an object, a hash key and a list element
sub lvalues() {
    list<auto> src = (1, 2, 3);
    Tracked o("o");
    o.holder = {};
    o.holder.l := src;
    push o.holder.l, 4;
    hash<auto> h = {};
    h.k := src;
    pop h.k;
    list<auto!> l = (NOTHING,);
    l[0] := src;
    shift l[0];
    printf("lvalues: %y\n", src);
    delete o;
}

#! the recursive-reference scan sees the objects added and removed through the weak reference
sub scans() {
    # an object added to the list that its member holds: the cycle is collected
    {
        Tracked c("cycle");
        list<auto!> inner = (NOTHING,);
        auto w;
        w := inner;
        w[0] := inner;
        remove w;
        c.holder = {"inner": inner};
        remove inner;
        push c.holder.inner[0], c;
    }
    printf("cycle destroyed: %y\n", destroyed);
    # an object removed through the weak reference is released
    {
        list<auto> src = (new Tracked("removed"),);
        list<auto> w;
        w := src;
        pop w;
    }
    printf("removed destroyed: %y\n", destroyed);
}

#! the operators on plain local variables, outside of closures
sub plain_locals() {
    list<auto> src = (1, 2, 3, 4);
    auto w;
    w := src;
    push w, 5;
    unshift w, 0;
    auto p = pop w;
    auto sh = shift w;
    splice w, 0, 1;
    auto ex = extract w, 0, 1;
    w += (6,);
    auto r = remove w[0..0];
    printf("plain locals: %y %y %y %y / %y\n", p, sh, ex, r, src);
    hash<auto> hsrc = {"a": 1, "b": 2, "c": 3};
    auto hw;
    hw := hsrc;
    hw -= "a";
    delete hw.b;
    hw += {"d": 4};
    printf("plain hash locals: %y\n", hsrc);
}

#! thread-local variables, which any code of the thread can assign a weak reference
sub thread_locals() {
    list<auto> src = (1, 2);
    tw := src;
    tw += (3,);
    push tw, 4;
    hash<auto> hsrc = {"a": 1, "b": 2};
    th := hsrc;
    th += {"c": 3};
    th -= "a";
    printf("thread_local: %y %y\n", src, hsrc);
    # a closure-bound variable
    list<auto> csrc = (1,);
    hash<auto> chsrc = {"a": 1};
    auto cw;
    auto chw;
    code c = sub () { cw += (2,); chw += {"b": 2}; chw -= "a"; };
    cw := csrc;
    chw := chsrc;
    c();
    printf("closure: %y %y\n", csrc, chsrc);
}

#! a typed list's element is not removed through a weak reference held by a member, a hash key or a list element
sub typed_removal() {
    list<int> ints = (1, 2, 3);
    Tracked o("typed");
    o.holder = {};
    o.holder.k := ints;
    printf("member key holder: %s / %y (%s)\n", run(string sub () { return sprintf("%y", remove o.holder.k[0]); }),
        ints, ints.fullType());
    printf("member key holder delete: %s / %y\n", run(string sub () { delete o.holder.k[1]; return "deleted"; }),
        ints);
    list<auto!> l = (NOTHING,);
    l[0] := ints;
    printf("element holder: %s / %y\n", run(string sub () { return sprintf("%y", remove l[0][0]); }), ints);
    delete o;
}

#! a weak reference to a hash under a list operator raises the operator's error, as for a hash
sub hash_under_list_ops() {
    hash<auto> hsrc = {"a": 1};
    auto hw;
    hw := hsrc;
    printf("hash push: %s\n", run(string sub () { push hw, 1; return "pushed"; }));
    printf("hash unshift: %s\n", run(string sub () { unshift hw, 1; return "unshifted"; }));
    printf("hash splice: %s\n", run(string sub () { splice hw, 0, 1; return "spliced"; }));
    printf("hash extract: %s\n", run(string sub () { extract hw, 0, 1; return "extracted"; }));
    remove hsrc;
    printf("freed hash push: %s\n", run(string sub () { push hw, 1; return "pushed"; }));
    printf("freed hash pop: %s\n", run(string sub () { pop hw; return "popped"; }));
    printf("freed hash shift: %s\n", run(string sub () { shift hw; return "shifted"; }));
    printf("freed hash splice: %s\n", run(string sub () { splice hw, 0, 1; return "spliced"; }));
}

#! a copy that shares the container (b = a) sees the changes made through the weak reference
sub shared_copy() {
    list<auto> a = (1, 2);
    auto w;
    w := a;
    list<auto> b = a;
    push w, 3;
    printf("shared copy: a %y b %y\n", a, b);
}

#! the entry of the list that holds the weak reference to the list itself is removed through it
sub self_entry() {
    list<auto!> inner = (NOTHING, 2, 3);
    auto w;
    w := inner;
    w[0] := inner;
    remove w;
    # the operator changes the list through the weak reference held by its first entry, and removes that entry
    auto first = shift inner[0];
    printf("self entry shift: %s %y\n", first.typeCode() == NT_LIST ? "the list" : sprintf("%y", first),
        inner.size());
    list<auto!> inner2 = (1, 2, NOTHING);
    auto w2;
    w2 := inner2;
    w2[2] := inner2;
    remove w2;
    auto last = pop inner2[2];
    printf("self entry pop: %s %y\n", last.typeCode() == NT_LIST ? "the list" : sprintf("%y", last), inner2);
}

lists();
hashes();
lvalues();
plain_locals();
thread_locals();
typed_removal();
hash_under_list_ops();
shared_copy();
self_entry();
scans();
