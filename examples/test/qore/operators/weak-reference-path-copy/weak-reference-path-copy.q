#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# A write through an lvalue path that reaches a container through a weak reference and later again through the
# containers holding it changes the container that the path designates (qore#7352): the reference that the step
# through the weak reference holds does not make the container look shared to the later step, which copied it and
# left the weak references on the original, which was then freed.  A container that another owner holds is still
# copied.

%modern
%allow-weak-references

our int destroyed = 0;

class C {
    public {
        string name;
        hash<auto> holder;
        auto lst;
    }

    constructor(string n) {
        self.name = n;
    }

    destructor() {
        ++destroyed;
    }
}

# holder, a, b, c, a (through wg), b, c, holder (through wh), a, b, c, a (through wg), b, c, t
sub three_weak_steps() {
    C c("c1");
    c.holder.a.b.c.t = {};
    auto w;
    w := c.holder.a.b.c;
    w.wg := c.holder.a;
    w.wh := c.holder;
    c.holder.a.b.c.t.marker = 1;
    c.holder.a.b.c.wg.b.c.wh.a.b.c.wg.b.c.t.m2 = 1;
    printf("three weak steps: path %y, weak path %y\n", c.holder.a.b.c.t, c.holder.a.b.c.wg.b.c.wh.a.b.c.wg.b.c.t);
    c.holder.a.b.c.wg.b.c.wh.a.b.c.t.m3 = 1;
    c.holder.a.b.c.wh.a.b.c.wg.b.c.t.m4 = 1;
    printf("then: %y\n", c.holder.a.b.c.t);
    remove c.holder;
}

# holder, a, b, holder (through wk), a, b, a (through wa), b, t: an object stored, replaced and removed
sub two_weak_steps_object() {
    int start = destroyed;
    {
        C c("c2");
        c.holder.a.b.t = {};
        auto w;
        w := c.holder.a.b;
        w.wk := c.holder;
        w.wa := c.holder.a;
        c.holder.a.b.wk.a.b.wa.b.t.k = new C("o1");
        c.holder.a.b.wa.b.wk.a.b.t.k = new C("o2");
        printf("two weak steps: %y, through the weak path: %y\n", c.holder.a.b.t.k.name,
            c.holder.a.b.wa.b.wk.a.b.t.k.name);
        c.holder.a.b.wa.b.wk.a.b.t.k = 1;
        printf("replaced: %y\n", c.holder.a.b.t);
        remove c.holder;
    }
    printf("objects destroyed: %d\n", destroyed - start);
}

# a list reached through a weak reference and again through the hash holding it
sub list_weak_steps() {
    C c("c3");
    c.holder.l = new list<auto>((NOTHING, NOTHING));
    c.holder.l[1] = new hash<auto>();
    c.holder.l[1].t = new hash<auto>();
    auto w;
    w := c.holder.l[1];
    w.wh := c.holder;
    auto wl;
    wl := c.holder.l;
    w.wl := wl;
    c.holder.l[1].wl[1].wh.l[1].t.k = 1;
    printf("list: path %y, weak path %y\n", c.holder.l[1].t, c.holder.l[1].wl[1].wh.l[1].t);
    c.holder.l[1].wh.l[1].t.k2 = 2;
    printf("list then: %y\n", c.holder.l[1].t);
    remove c.holder;
}

# a local variable as the root of the path
sub local_root() {
    hash<auto> h = {};
    h.a.b.c.t = {};
    auto w;
    w := h.a.b.c;
    w.wg := h.a;
    w.wh := h;
    h.a.b.c.wg.b.c.wh.a.b.c.wg.b.c.t.k = 1;
    printf("local root: path %y, weak path %y\n", h.a.b.c.t, h.a.b.c.wg.b.c.wh.a.b.c.wg.b.c.t);
}

# a container on the path that another variable holds is copied: the variable keeps the original, which the weak
# references keep designating
sub shared_container_copied() {
    C c("c4");
    c.holder.a.b.t = {};
    auto w;
    w := c.holder.a.b;
    w.wk := c.holder;
    hash<auto> other = c.holder.a;
    c.holder.a.b.wk.a.b.t.k = 1;
    printf("shared: path %y, other %y, weak path %y\n", c.holder.a.b.t, other.b.t, c.holder.a.b.wk.a.b.t);
    remove c.holder;
}

# the operations that remove from or reorder a hash or a list reached again after a weak step, which require it to be
# unique
sub hash_removals() {
    hash<auto> h = {};
    h.a.b = {};
    h.a.z = 1;
    h.a.y = 2;
    h.a.x = 3;
    auto w;
    w := h.a.b;
    w.wg := h.a;
    w.wh := h;
    delete h.a.b.wg.b.wh.a.z;
    auto y = remove h.a.b.wg.b.wh.a.y;
    auto slice = remove h.a.b.wg.b.wh.a{"x", "missing"};
    printf("hash removals: keys %y, weak keys %y, removed %y and %y\n", keys h.a, keys h.a.b.wg, y, slice);
}

sub list_operations() {
    hash<auto> h = {};
    h.l = new list<auto>();
    h.l += ({}, 1, 2, 3, 4, 5, 6);
    auto w;
    w := h.l[0];
    w.wh := h;
    auto wl;
    wl := h.l;
    w.wl := wl;
    auto last = pop h.l[0].wl[0].wh.l;
    push h.l[0].wl[0].wh.l, 9;
    splice h.l[0].wl[0].wh.l, 1, 1;
    auto e = extract h.l[0].wl[0].wh.l, 1, 1;
    delete h.l[0].wl[0].wh.l[2];
    printf("list operations: popped %y, extracted %y, list %y, through the weak list %y\n", last, e, h.l[1..],
        wl[1..]);
    # the first element holds the weak references that the path goes through: it is shifted out last
    auto first = shift h.l[0].wl[0].wh.l;
    printf("list shift: %y, size %d, through the weak list %d\n", first.typeCode(), h.l.size(), wl.size());
}

# the operation replaces or deletes the container that the path reached again, after the helper released its own
# references to it
sub reached_container_replaced() {
    hash<auto> g = {};
    g.a.b = {};
    g.a.x = {"v": 1};
    auto w;
    w := g.a.b;
    w.wg := g.a;
    w.wh := g;
    g.a.b.wg.b.wh.a.b.wg.b.wh.a.x.v = 7;
    printf("reached twice: %y\n", g.a.x);
    g.a.b.wg.b.wh.a.b.wh.a = {"n": 1};
    printf("replaced: %y\n", g.a);
    g.a = new hash<auto>();
    g.a.b = new hash<auto>();
    g.a.x = 1;
    w := g.a.b;
    w.wg := g.a;
    w.wh := g;
    delete g.a.b.wg.b.wh.a.b.wh.a.x;
    printf("deleted from it: %y, through the weak references: %y\n", keys g.a, keys g.a.b.wg);
}

const Cases = {
    "three_weak_steps": \three_weak_steps(),
    "two_weak_steps_object": \two_weak_steps_object(),
    "list_weak_steps": \list_weak_steps(),
    "local_root": \local_root(),
    "shared_container_copied": \shared_container_copied(),
    "hash_removals": \hash_removals(),
    "list_operations": \list_operations(),
    "reached_container_replaced": \reached_container_replaced(),
};

# a case named on the command line runs alone
*string only = ARGV[0];
foreach hash<auto> i in (Cases.pairIterator()) {
    if (!only || only == i.key) {
        try {
            call_function(i.value);
        } catch (hash<ExceptionInfo> ex) {
            printf("%s: %s: %s\n", i.key, ex.err, ex.desc);
        }
    }
}
printf("done\n");
