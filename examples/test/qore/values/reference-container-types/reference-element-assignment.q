#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# assigning an element of a list of references held in a hash assigns the referenced variable (qore#6694)

%modern

our int g = 1;

sub show(string label, auto v) {
    printf("%s: %y\n", label, v);
}

sub run_case(string label, code c) {
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

# the case of the issue, in top-level code
hash h = {"x": (\g,)};
h.x[0] = 5;
show("top-level h.x[0] = 5", g);

run_case("untyped hash", sub () {
    g = 1;
    hash h1 = {"x": (\g,)};
    h1.x[0] = 3;
    show("untyped hash h.x[0] = 3", g);
});
run_case("auto hash", sub () {
    g = 1;
    hash<auto> h1 = {"x": (\g,)};
    h1.x[0] = 4;
    show("hash<auto> h.x[0] = 4", g);
});
run_case("declared hash type", sub () {
    g = 1;
    hash<string, list<reference<int>>> h1 = {"x": (\g,)};
    h1.x[0] = 5;
    show("hash<string, list<reference<int>>> h.x[0] = 5", g);
});
run_case("key expression", sub () {
    g = 1;
    hash h1 = {"x": (\g,)};
    string k = "x";
    h1{k}[0] = 6;
    show("h{k}[0] = 6", g);
});
run_case("nested hash", sub () {
    g = 1;
    hash h1 = {"a": {"x": (\g,)}};
    h1.a.x[0] = 7;
    show("h.a.x[0] = 7", g);
});
run_case("second element", sub () {
    int a = 1;
    int b = 2;
    hash h1 = {"x": (\a, \b)};
    h1.x[1] = 8;
    show("h.x[1] = 8: a", a);
    show("h.x[1] = 8: b", b);
});
run_case("compound assignment", sub () {
    g = 1;
    hash h1 = {"x": (\g,)};
    h1.x[0] += 8;
    show("h.x[0] += 8", g);
});
run_case("local variable referenced", sub () {
    int v = 1;
    hash h1 = {"x": (\v,)};
    h1.x[0] = 10;
    show("local v after h.x[0] = 10", v);
    show("h.x[0] reads", h1.x[0]);
});
run_case("type error", sub () {
    g = 1;
    hash h1 = {"x": (\g,)};
    h1.x[0] = "str";
    show("h.x[0] = \"str\"", g);
});
