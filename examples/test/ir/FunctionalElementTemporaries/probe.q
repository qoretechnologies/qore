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

# a value read from a local that a closure parsed after the operator binds: each element's is released at its end
our *code lgc;
class L {
    private { string n; }
    constructor(string n) { self.n = n; printf("new %s\n", n); }
    destructor() { printf("~%s\n", n); }
    string get(auto i) { printf("get %s\n", n); if (lgc) { lgc(i); } return "k" + i; }
}
sub late_bound(list<int> l) {
    *L o = new L("u0");
    for (int pass = 0; pass < 2; ++pass) {
        if (pass) {
            show("late bound", (map {o.get($1): $1}, l));
        }
        lgc = sub (auto i) { o = new L("u" + i); };
    }
    lgc = NOTHING;
}
# the same in a generic function, whose operator is replaced by a generic one
sub late_bound_generic<T>(list<T> l) {
    *L o = new L("v0");
    for (int pass = 0; pass < 2; ++pass) {
        if (pass) {
            show("late bound generic", (map {o.get($1): $1}, l));
        }
        lgc = sub (auto i) { o = new L("v" + i); };
    }
    lgc = NOTHING;
}
# the same in a closure of a constant, which is initialized after the code of the Program
const LateBoundConst = sub (list<int> l) {
    *L o = new L("w0");
    for (int pass = 0; pass < 2; ++pass) {
        if (pass) {
            show("late bound const", (map {o.get($1): $1}, l));
        }
        lgc = sub (auto i) { o = new L("w" + i); };
    }
    lgc = NOTHING;
};
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

sub h() {
    list<int> l = (1, 2, 3);
    show("fn foldl-over-map", (foldl $1 + $2, (map (new O("u" + $1)).get(), l)) + (foldl $1 + (new O("v" + $2)).get(), (map $1, l)));
    show("fn hash-map-single", (map {(new O("hk")).get(): (new O("hv")).get()}, 1));
    show("fn any", (any (new O("y" + $1)).get() > 5, l));
    show("fn count", (count (new O("z" + $1)).get() > 0, l));
    show("fn takewhile", (takewhile (new O("w" + $1)).get() > 0, l));
    show("fn any-over-map", (any $1 > 5, (map (new O("j" + $1)).get(), l)));
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
h();
late_bound((1, 2));
late_bound_generic<int>((1, 2));
LateBoundConst((1, 2));
