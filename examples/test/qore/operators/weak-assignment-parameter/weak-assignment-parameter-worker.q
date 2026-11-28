#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# a weak reference to a parameter passed by value designates the call's own copy of the value; a weak reference to a
# parameter passed by reference designates the caller's variable

%modern
%allow-weak-references

our hash<string, int> gh;
our list<int> gl;

sub hash_by_value(hash<string, int> h) { gh := h; }
sub hash_by_reference(reference<hash<string, int>> h) { gh := h; }
sub list_by_value(list<int> l) { gl := l; }
sub list_by_reference(reference<list<int>> l) { gl := l; }

class Holder {
    public { hash<string, int> w; }
    bind(reference<hash<string, int>> h) { w := h; }
}

sub check(string name, auto expected, auto actual) {
    printf("%s %s: %y\n", expected === actual ? "PASS" : "FAIL", name, actual);
}

hash<string, int> h = {"a": 1};
hash_by_value(h);
h.b = 2;
check("hash by value: the call's copy is released", {}, gh);

h = {"a": 1};
hash_by_reference(\h);
h.b = 2;
check("hash by reference: designates the caller's variable", {"a": 1, "b": 2}, gh);
gh.c = 3;
check("hash by reference: a change through the weak reference", {"a": 1, "b": 2, "c": 3}, h);

list<int> l = (1,);
list_by_value(l);
push l, 2;
check("list by value: the call's copy is released", (), gl);

l = (1,);
list_by_reference(\l);
push l, 2;
check("list by reference: designates the caller's variable", (1, 2), gl);

Holder holder();
hash<string, int> m = {"x": 1};
holder.bind(\m);
m.y = 2;
check("method parameter by reference", {"x": 1, "y": 2}, holder.w);
