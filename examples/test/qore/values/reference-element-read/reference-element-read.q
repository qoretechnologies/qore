#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# a member or an element of a reference held in a container reads the referenced value, as does a typed assignment of
# it (qore#6662)

%modern

class T {
    public {
        string n;
        int x = 5;
    }

    constructor(string n) {
        self.n = n;
    }
}

class Holder {
    public {
        int m;
    }
}

hashdecl HD {
    int m = 3;
}

our *T g_p = new T("p");
our int g_i = 7;
our hash<auto> g_h = {"n": "hv", "m": 2};
our hash<string, int> g_hi = {"n": 4, "m": 6};
our hash<HD> g_hd();
our list<int> g_l = (10, 20, 30, 40);
our int g_x;

sub show(string label, code c) {
    try {
        printf("%s: %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

our int g_next = 2;

int sub next_index() {
    return ++g_next;
}

sub take_reference(reference<int> r) {
    r = 8;
}

sub run() {
    hash<string, reference<*T>> ho = {"a": \g_p};
    hash<string, reference<hash<auto>>> hh = {"a": \g_h};
    hash<string, reference<hash<string, int>>> hi = {"a": \g_hi};
    hash<string, reference<hash<HD>>> hd = {"a": \g_hd};
    list<reference<list<int>>> ll = (\g_l,);
    list<reference<int>> li = (\g_i,);
    int i = 1;

    # a member or an element of the referenced value
    show("object member", auto sub () { return ho.a.n; });
    show("object member in braces", auto sub () { return ho.a{"x"}; });
    show("hash member", auto sub () { return hh.a.n; });
    show("hash slice", auto sub () { return hh.a{("n", "m")}; });
    show("member of a hash of int", auto sub () { return hi.a.n; });
    show("hashdecl member", auto sub () { return hd.a.m; });
    show("list element", auto sub () { return ll[0][1]; });
    show("negative list offset", auto sub () { return ll[0][-1]; });
    show("list element at a variable offset", auto sub () { return ll[0][i]; });
    show("list range", auto sub () { return ll[0][1..2]; });
    show("list elements", auto sub () { return ll[0][(0, 2)]; });
    show("list range and element", auto sub () { return ll[0][0..1, 3]; });

    # a typed assignment reads the referenced value
    show("typed declaration of an object", auto sub () { *T o = ho.a; return o.n; });
    show("typed declaration of an int", auto sub () { int x = li[0]; return x; });
    show("typed declaration of a list", auto sub () { list<int> x = ll[0]; return x; });
    show("assignment to a typed local", auto sub () { int x; x = li[0]; return x; });
    show("assignment to a typed global", auto sub () { g_x = li[0]; return g_x; });
    show("assignment to a typed member", auto sub () { Holder h(); h.m = li[0]; return h.m; });
    show("assignment to an element of a typed hash", auto sub () { hash<string, int> t = {}; t.x = li[0]; return t; });
    show("assignment to an element of a typed list", auto sub () { list<int> t = (0,); t[0] = li[0]; return t; });
    show("assignment to a closure-bound typed local", auto sub () {
        int x;
        code f = sub () { x = li[0]; };
        f();
        return x;
    });
    # the referenced variable is the target: it is read before the target is locked
    show("assignment to the referenced variable", auto sub () { g_i = li[0]; return g_i; });
    # the referenced lvalue does not exist any more
    show("member of a deleted object", auto sub () {
        *T d = new T("d");
        hash<string, reference<*T>> hd2 = {"a": \d};
        delete d;
        *T o = hd2.a;
        return o;
    });

    # an object member whose object was deleted: the exception is raised by the read
    show("member of a deleted object", auto sub () {
        T obj("o");
        hash<string, reference<string>> hm = {"a": \obj.n};
        T alias = obj;
        delete alias;
        try {
            string s = hm.a;
            return s;
        } catch (hash<ExceptionInfo> ex) {
            return ex.err;
        }
    });
    # a referenced hash key that does not exist has no value
    show("missing referenced key", auto sub () {
        hash<string, int> hs = {};
        list<reference<*int>> lr = (\hs.k,);
        *int x = lr[0];
        return x;
    });
    show("missing referenced key for an int", auto sub () {
        hash<string, int> hs = {};
        list<reference<int>> lr = (\hs.k,);
        int x = lr[0];
        return x;
    });
    # a list of selectors with a range and a selector that can have side effects
    show("range and a selector with side effects", auto sub () {
        g_next = 1;
        return ll[0][0..1, next_index()];
    });
    show("iteration of a range and a selector with side effects", auto sub () {
        list<int> rv = ();
        g_next = 1;
        foreach int v in (ll[0][0..1, next_index()]) {
            push rv, v;
        }
        return rv;
    });
    show("map of a range and a selector with side effects", auto sub () {
        g_next = 1;
        return map $1, ll[0][0..1, next_index()];
    });
    # the referenced value is read once, before the selectors change the variable that it names
    show("selector that changes the referenced variable", auto sub () {
        list<int> l = (1, 2, 3);
        hash<string, reference<list<int>>> hr = {"a": \l};
        return hr.a[0, (l = (7, 8, 9)) ? 1 : 1, 2..2];
    });
    # the order in which the selectors and the mapped expression run
    show("order of the selectors and of the map", auto sub () {
        list<string> order = ();
        list<int> l = (1, 2, 3, 4);
        hash<string, reference<list<int>>> hr = {"a": \l};
        code f = int sub (int i) { push order, sprintf("f%d", i); return i; };
        code g = int sub (int v) { push order, sprintf("g%d", v); return v; };
        list<int> rv = map g($1), hr.a[f(0), f(1)..f(2)];
        return (rv, order);
    });
    # a reference to an element that holds a reference itself: the reference that it holds
    show("reference to a reference element", auto sub () {
        hash<auto> inner = {"b": \g_l};
        list<reference<hash<auto>>> lh = (\inner,);
        return lh[0].b[1];
    });

    # the reference as it is
    show("operator", auto sub () { return li[0] + 1; });
    show("auto variable", auto sub () {
        auto w = li[0];
        w = 9;
        int rv = g_i;
        g_i = 7;
        return rv;
    });
    show("reference parameter", auto sub () {
        take_reference(li[0]);
        int rv = g_i;
        g_i = 7;
        return rv;
    });
    show("iteration", auto sub () {
        int count = 0;
        foreach auto v in (ll[0]) {
            ++count;
        }
        return count;
    });
    # a container whose type does not say that it holds references: its element is assigned as it is
    show("typed assignment from an untyped container", auto sub () {
        hash<auto> u = {"r": \g_i};
        int x = u.r;
        return x;
    });
    show("member access in an untyped container", auto sub () {
        hash<auto> u = {"r": \g_l};
        return u.r[1];
    });
}

for (int k = 0; k < 2; ++k) {
    g_next = 2;
    run();
}
