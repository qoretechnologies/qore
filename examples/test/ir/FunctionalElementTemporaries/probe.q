#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# the probe of FunctionalElementTemporaries.qtest: the temporary values that a functional operator's expression creates
# for an element are destroyed once the element's value has been computed, before the next element, in every
# execution mode (qore#6288)

%modern

class O {
    private { string n; }
    constructor(string n) { self.n = n; printf("new %s\n", n); }
    destructor() { printf("~%s\n", n); }
    int get() { printf("get %s\n", n); return n.size(); }
}
class E {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { throw "DTOR-" + n; }
    int get() { printf("get %s\n", n); return 1; }
}
sub show(string label, auto v) { printf("%s: %y\n", label, v); }
list<int> l = (1, 2, 3);
show("top map", (map (new O("m" + $1)).get(), l));
show("top map-select", (map (new O("s" + $1)).get(), l, (new O("p" + $1)).get() > 0));
show("top select", (select l, (new O("c" + $1)).get() > 0));
show("top foldl", (foldl $1 + (new O("f" + $2)).get(), l));
show("top foldr", (foldr $1 + (new O("r" + $2)).get(), l));
show("top hash-map", (map {"k" + $1: (new O("h" + $1)).get()}, l));
show("top hash-map-select", (map {"k" + $1: (new O("x" + $1)).get()}, l, (new O("q" + $1)).get() > 0));

sub f() {
    list<int> l = (1, 2, 3);
    show("fn map", (map (new O("m" + $1)).get(), l));
    show("fn map-over-select", (map (new O("a" + $1)).get(), (select l, (new O("b" + $1)).get() > 0)));
    show("fn select-over-map", (select (map (new O("c" + $1)).get() + $1, l), $1 > 0));
    show("fn foldl-over-map", (foldl $1 + $2, (map (new O("d" + $1)).get(), l)));
    show("fn map-iterator", (map (new O("i" + $1)).get(), new ListIterator(l)));
    foreach int v in (map (new O("e" + $1)).get(), l) {
        printf("body %d\n", v);
    }
    show("fn map-map", (map $1 + 1, (map (new O("g" + $1)).get(), l)));
}

sub g() {
    list<int> l = (1, 2, 3);
    # a destructor that raises an exception: the first element's temporary raises it before the next element
    try {
        show("fn map raising", (map (new E("e" + $1)).get(), l));
    } catch (hash<ExceptionInfo> ex) {
        printf("fn map raising: caught %s\n", ex.err);
    }
    try {
        show("fn select raising", (select l, (new E("s" + $1)).get() > 0));
    } catch (hash<ExceptionInfo> ex) {
        printf("fn select raising: caught %s\n", ex.err);
    }
}
f();
g();
