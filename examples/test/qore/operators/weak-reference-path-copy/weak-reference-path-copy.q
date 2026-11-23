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

const Cases = {
    "three_weak_steps": \three_weak_steps(),
    "two_weak_steps_object": \two_weak_steps_object(),
    "list_weak_steps": \list_weak_steps(),
    "local_root": \local_root(),
    "shared_container_copied": \shared_container_copied(),
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
