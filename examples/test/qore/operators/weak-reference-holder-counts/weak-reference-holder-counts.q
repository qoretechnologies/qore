#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# A change through a weak reference that adds an object to the list or hash that the weak reference designates keeps
# the object counts of the containers and objects that hold that container, which are not on the lvalue's path
# (qore#7318): a later removal does not subtract from a count of 0 (an assertion failure in a debug build), and a cycle
# made through such a change is collected.

%modern
%allow-weak-references

class C {
    public {
        string name;
        hash<auto> holder;
        auto ref;
    }

    constructor(string n) {
        name = n;
    }

    destructor() {
        printf("destroyed %s\n", name);
    }
}

# an element write through the weak reference, then the removal of the list from its holder
sub list_element_then_remove() {
    C c("c1");
    c.holder = {"inner": new list<auto!>((NOTHING,))};
    auto w;
    w := c.holder.inner;
    w[1] = new C("x1");
    printf("list element: %y\n", c.holder.inner[1].name);
    remove c.holder.inner;
    printf("list element: removed: %y\n", c.holder.inner);
}

# a key write through the weak reference, then the removal of the hash from its holder
sub hash_key_then_remove() {
    C c("c2");
    c.holder = {"inner": {}};
    auto w;
    w := c.holder.inner;
    w.o = new C("x2");
    printf("hash key: %y\n", c.holder.inner.o.name);
    remove c.holder.inner;
    printf("hash key: removed: %y\n", c.holder.inner);
}

# += on a list reached through the weak reference (qore#7297), then the removal
sub append_then_remove() {
    C c("c3");
    c.holder = {"inner": {"l": ()}};
    auto w;
    w := c.holder.inner;
    w.l += new C("x3");
    printf("append: %y\n", c.holder.inner.l[0].name);
    remove c.holder.inner;
    printf("append: removed: %y\n", c.holder.inner);
}

# a cycle made through the weak reference is collected when its root goes out of scope
sub cycle_through_weak_write() {
    {
        C c("c4");
        c.holder = {"inner": {}};
        auto w;
        w := c.holder.inner;
        C x("x4");
        x.ref = c;
        w.o = x;
    }
    printf("cycle: scope left\n");
}

# the object that a weak write stores is removed through the weak reference, then the holder is removed
sub weak_write_then_weak_remove() {
    C c("c5");
    c.holder = {"inner": {}};
    auto w;
    w := c.holder.inner;
    w.o = new C("x5");
    remove w.o;
    printf("weak remove: %y\n", c.holder.inner);
    remove c.holder.inner;
    printf("weak remove: removed: %y\n", c.holder.inner);
}

# a list slice removed from a list that a weak reference pins (the list still needs a scan): its holder keeps
# counting it, and the removal of the list from its holder is counted right
sub slice_remove_from_pinned_list() {
    C c("c6");
    c.holder = {"l": new list<auto!>((1, 2, 3))};
    auto w;
    w := c.holder.l;
    c.holder.l[1] = new C("x6");
    remove c.holder.l[1, 2];
    list<int> k = (0,);
    remove c.holder.l[k];
    printf("slice remove: %y, holder count consistent: %y\n", c.holder.l, check_count(\c.holder, 1));
    remove c.holder.l;
    printf("slice remove: removed: %y\n", c.holder.l);
}

class Hd {
    public {
        hash<auto> h = {};
    }

    hash<auto> get() {
        return h;
    }
}

# a weak reference made from an expression (a call result that the object also holds) designates a copy of its own,
# a temporary of the statement: a change through it later raises HASH-ERROR and does not change the object's member,
# and the object's counts stay right (qore#7318)
sub expression_source_copy() {
    Hd hd();
    auto w;
    w := hd.get();
    try {
        w.o = new C("x7");
        printf("expression: written\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("expression: %s\n", ex.err);
    }
    printf("expression: member: %y\n", hd.h);
    remove hd.h;
    printf("expression: removed: %y\n", hd.h);
}

# a path that a slice stops is read as an expression: the weak reference designates a copy of its own
sub slice_path_source_copy() {
    C c("c8");
    c.holder = {"a": {}};
    list<string> k = ("a",);
    auto w;
    w := c.holder{k}.a;
    try {
        w.o = new C("x8");
        printf("slice path: written\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("slice path: %s\n", ex.err);
    }
    printf("slice path: member: %y\n", c.holder.a);
    remove c.holder.a;
    printf("slice path: removed: %y\n", c.holder.a);
}

class Gated {
    public {
        hash<auto> real = {};
    }

    auto memberGate(string m) {
        return "gate:" + m;
    }
}

# a declared member of a class with memberGate() is read without memberGate(): it is made unique as an lvalue, so the
# literal that initializes it is not changed through the weak reference
sub member_gate_declared_member() {
    Gated g1();
    auto w;
    w := g1.real;
    w.added = 1;
    Gated g2();
    printf("memberGate class: g1: %y, g2: %y, gated: %y\n", g1.real, g2.real, g1.other);
}

# true if the object count of the container that \a v names is as expected (dbg_get_scan_count() exists in debug
# builds only); the container is read through the reference, as a container that a weak reference designates is passed
# to a function as a copy of its own
bool sub check_count(reference<auto> v, int expected) {
    return exists_function("dbg_get_scan_count") ? call_function("dbg_get_scan_count", \v) == expected : True;
}

#! runs a case by its name: each case is called by name, so that JIT and tiered execution run it as native code
#! (a call through a call reference runs on the IR tier, and is compiled only when it returns)
sub run_case(string name) {
    switch (name) {
        case "list_element_then_remove": list_element_then_remove(); break;
        case "hash_key_then_remove": hash_key_then_remove(); break;
        case "append_then_remove": append_then_remove(); break;
        case "cycle_through_weak_write": cycle_through_weak_write(); break;
        case "weak_write_then_weak_remove": weak_write_then_weak_remove(); break;
        case "slice_remove_from_pinned_list": slice_remove_from_pinned_list(); break;
        case "expression_source_copy": expression_source_copy(); break;
        case "slice_path_source_copy": slice_path_source_copy(); break;
        case "member_gate_declared_member": member_gate_declared_member(); break;
    }
}

const CaseNames = (
    "list_element_then_remove",
    "hash_key_then_remove",
    "append_then_remove",
    "cycle_through_weak_write",
    "weak_write_then_weak_remove",
    "slice_remove_from_pinned_list",
    "expression_source_copy",
    "slice_path_source_copy",
    "member_gate_declared_member",
);

# a case named on the command line runs alone
*string only = ARGV[0];
foreach string n in (CaseNames) {
    if (!only || only == n) {
        try {
            run_case(n);
        } catch (hash<ExceptionInfo> ex) {
            printf("%s: %s: %s\n", n, ex.err, ex.desc);
        }
    }
}
printf("done\n");
