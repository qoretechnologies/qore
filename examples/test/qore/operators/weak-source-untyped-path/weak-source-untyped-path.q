#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# The source of a weak assignment that is a path is made unique, so that the weak reference designates the container
# that the path holds, also when the steps of the path have no type at parse time (qore#7351): a container literal of
# the program that the path shares is copied first, so a change through the weak reference never changes the literal,
# and a later write through the path changes the container that the weak reference designates.  A step that an lvalue
# cannot take (a slice, a member read through memberGate(), an index of a string) reads the value as before.

%modern
%allow-weak-references

class C {
    public {
        hash<auto> holder;
    }
}

class GatedMembers {
    public {
        int n = 1;
        int calls = 0;
    }

    auto memberGate(string m) {
        ++calls;
        return {"gated": m};
    }
}

# a class whose private member is read as an lvalue in the class, and as the operator reads it outside
class Priv {
    private {
        hash<auto> p;
    }

    setLiteral() {
        p = {"q": {}};
    }

    auto get() {
        return p;
    }

    # in the class the private member is an lvalue: the literal is copied before the weak reference is taken
    auto weakInside(auto o) {
        auto w;
        w := o.p.q;
        w.mark = 1;
        return p.q.mark;
    }
}

int key_evals = 0;
string sub key(string k) {
    ++key_evals;
    return k;
}

# the literal assigned to the member is shared with the program until it is changed
sub literal_through_member(int run) {
    C c();
    c.holder = {"z": {"v": {"t": {}}, "v2": {}}};
    printf("member, run %d: the literal as assigned: %y\n", run, c.holder);
    auto w;
    w := c.holder.z.v;
    w.mark = {"run": run};
    printf("member, run %d: through the path: %y\n", run, c.holder.z.v.mark);
}

sub literal_through_local(int run) {
    hash<auto> h = {"z": {"v": {}}};
    printf("local, run %d: the literal as assigned: %y\n", run, h);
    auto w;
    w := h.z.v;
    w.mark = {"run": run};
    printf("local, run %d: through the path: %y\n", run, h.z.v.mark);
}

# qore#7351: a weak reference to an ancestor held by the container, then a weak reference to the container
sub ancestor_then_container() {
    C c();
    c.holder = {"z": {"v": {"t": {}}, "v2": {}}};
    auto w;
    w := c.holder.z.v;
    w.w1 := c.holder;
    auto w2;
    w2 := c.holder.z.v2;
    w2.w2 := c.holder.z.v;
    c.holder.z.v.t.marker = 1;
    printf("ancestor then container: through the path: %y, through w2: %y\n", c.holder.z.v.t, c.holder.z.v2.w2.t);
}

# the steps that an lvalue cannot take read the value as before
sub steps_read_as_before() {
    hash<auto> h = {"a": {"x": 1, "y": 2}, "s": "abc", "g": new GatedMembers()};
    auto w;
    w := h.a{"x", "y"};
    printf("slice: %y\n", w);
    w := h.g.anything;
    printf("memberGate: %y\n", w);
    w := h.s[1];
    printf("string index: %y\n", w);
    w := h.missing.key;
    printf("missing: %y\n", w);
}

# a root that has no type at parse time: the steps are checked when they run
sub untyped_object_root() {
    auto o = new C();
    o.holder = {"z": {"v": {}}};
    auto w;
    w := o.holder.z.v;
    w.mark = 1;
    printf("untyped object root: through the path: %y\n", o.holder.z.v.mark);
}

# a key that a step reads as the operator does is evaluated once, and memberGate() is called once
sub reads_once() {
    GatedMembers g();
    hash<auto> h = {"g": g, "a": {"x": {"y": 1}}};
    key_evals = 0;
    auto w;
    w := h{key("g")}{key("anything")}.gated;
    printf("reads once: %y, key evaluations: %d, memberGate calls: %d\n", w, key_evals, g.calls);
    key_evals = 0;
    list<string> ks = ("x",);
    w := h.a{ks}.x{key("y")};
    printf("reads once after a slice: %y, key evaluations: %d\n", w, key_evals);
}

# a private member is an lvalue in its class, and read as the operator reads it outside, where it is not accessible
sub private_member() {
    Priv p();
    # run twice: the literal that the member was assigned is not changed through the weak reference
    for (int run = 1; run <= 2; ++run) {
        p.setLiteral();
        printf("private member in the class, run %d: as assigned: %y\n", run, p.get());
        printf("private member in the class, run %d: through the path: %y\n", run, p.weakInside(p));
    }
    hash<auto> h = {"p": p};
    try {
        auto w;
        w := h.p.p;
        printf("private member outside the class: %y\n", w);
    } catch (hash<ExceptionInfo> ex) {
        printf("private member outside the class: %s: %s\n", ex.err, ex.desc);
    }
}

# the member of a deleted object reached through a weak reference is an lvalue of a deleted object, as for a path
# whose steps have types (qore#7242)
sub deleted_weak_object() {
    C c();
    hash<auto> h;
    h.o := c;
    delete c;
    try {
        auto w;
        w := h.o.holder;
        printf("deleted weak object: %y\n", w);
    } catch (hash<ExceptionInfo> ex) {
        printf("deleted weak object: %s\n", ex.err);
    }
}

# string and binary indexes, also negative ones, read as the operator reads them
sub string_and_binary_indexes() {
    hash<auto> h = {"s": "abc", "b": <0a0b0c>};
    auto w;
    w := h.s[-1];
    printf("string index -1: %y\n", w);
    w := h.b[1];
    printf("binary index 1: %y\n", w);
    w := h.s[1].x;
    printf("member of a string index: %y\n", w);
}

#! runs a case by its name: each case is called by name, so that JIT and tiered execution run it as native code
#! (a call through a call reference runs on the IR tier, and is compiled only when it returns)
sub run_case(string name) {
    switch (name) {
        case "literal_through_member": literal_through_member(1); literal_through_member(2); break;
        case "literal_through_local": literal_through_local(1); literal_through_local(2); break;
        case "ancestor_then_container": ancestor_then_container(); break;
        case "steps_read_as_before": steps_read_as_before(); break;
        case "untyped_object_root": untyped_object_root(); break;
        case "reads_once": reads_once(); break;
        case "private_member": private_member(); break;
        case "deleted_weak_object": deleted_weak_object(); break;
        case "string_and_binary_indexes": string_and_binary_indexes(); break;
    }
}

const CaseNames = (
    "literal_through_member",
    "literal_through_local",
    "ancestor_then_container",
    "steps_read_as_before",
    "untyped_object_root",
    "reads_once",
    "private_member",
    "deleted_weak_object",
    "string_and_binary_indexes",
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
