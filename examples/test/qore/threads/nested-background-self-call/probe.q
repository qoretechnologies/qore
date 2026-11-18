#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# the probe of nested-background-self-call.qtest: a background expression nested in a background expression calls
# methods of the object; the copy of the inner expression, made in the thread of the outer one, keeps the object of
# the starting code (qore#5971)

%modern

# counts down a Counter when it is released: the argument of a call is released when the call returns or raises, so
# the main thread waits for the call in the inner thread without polling
class Signal {
    private { Counter c; string n; }
    constructor(Counter c, string n) { self.c = c; self.n = n; }
    destructor() { c.dec(); }
}

class B {
    private { string name; }
    constructor(string name) { self.name = name; }
    int m(Signal s, int x) { printf("%s B::m(%d)\n", name, x); return x; }
    # prints nothing: the calls of the threads that are started at the same time are only counted
    int q(Signal s, int x) { return x; }
    private int pm(Signal s, int x) { printf("%s B::pm(%d)\n", name, x); return x; }
    static int sm(Signal s, int x) { printf("B::sm(%d)\n", x); return x; }
    kill(Signal s) { delete self; }
    # the object is deleted in the inner thread before the call, which raises OBJECT-ALREADY-DELETED there without
    # evaluating its arguments; the Signal of kill() is released before it
    nested_deleted(Counter c) { background (background (kill(new Signal(c, "d")), m(new Signal(c, "x"), 1))); }
    # the object is alive: the call is made in the inner thread
    nested_alive(Counter c) { background (background m(new Signal(c, "a"), 2)); }
    # three levels
    nested3(Counter c) { background (background (background m(new Signal(c, "3"), 3))); }
    # the inner expression is a list of calls
    nested_list(Counter c) { background (background (m(new Signal(c, "l1"), 4), m(new Signal(c, "l2"), 5))); }
    # a private method and a static method
    nested_private(Counter c) { background (background pm(new Signal(c, "p"), 6)); }
    nested_static(Counter c) { background (background sm(new Signal(c, "s"), 7)); }
    # the inner expression is in a closure of the outer one
    nested_closure(Counter c) { background (sub () { background m(new Signal(c, "c"), 8); })(); }
    # an argument of the inner call is a call through self
    nested_arg(Counter c) { background (background m(new Signal(c, "a1"), m(new Signal(c, "a2"), 9))); }
    # a map and a select of the outer expression start an inner thread for each element, and a call through self in
    # the map is evaluated for each element beside them
    nested_map(Counter c) { background (map background q(new Signal(c, "m"), $1), (1, 2)); }
    nested_map_both(Counter c) {
        background (map (q(new Signal(c, "mb1"), $1), background q(new Signal(c, "mb2"), $1)), (1, 2));
    }
    nested_select(Counter c) { background ign(select (1, 2), background q(new Signal(c, "s"), $1)); }
    # a map and a select in the inner expression
    inner_map(Counter c) { background (background ign(map q(new Signal(c, "im"), $1), (1, 2, 3))); }
    inner_select(Counter c) {
        background (background (background ign(select (1, 2), q(new Signal(c, "is"), $1))));
    }
}

sub ign(auto x) {}

sub run(string label, code c, int signals) {
    Counter cnt(signals);
    c(cnt);
    cnt.waitForZero();
    printf("%s done\n", label);
}

B b1("b1");
run("alive", sub (Counter c) { b1.nested_alive(c); }, 1);
run("three levels", sub (Counter c) { b1.nested3(c); }, 1);
run("list", sub (Counter c) { b1.nested_list(c); }, 2);
run("private", sub (Counter c) { b1.nested_private(c); }, 1);
run("static", sub (Counter c) { b1.nested_static(c); }, 1);
run("closure", sub (Counter c) { b1.nested_closure(c); }, 1);
run("argument", sub (Counter c) { b1.nested_arg(c); }, 2);
run("map", sub (Counter c) { b1.nested_map(c); }, 2);
run("map with a call", sub (Counter c) { b1.nested_map_both(c); }, 4);
run("select", sub (Counter c) { b1.nested_select(c); }, 2);
run("inner map", sub (Counter c) { b1.inner_map(c); }, 3);
run("inner select", sub (Counter c) { b1.inner_select(c); }, 2);
B b2("b2");
run("deleted", sub (Counter c) { b2.nested_deleted(c); }, 1);
