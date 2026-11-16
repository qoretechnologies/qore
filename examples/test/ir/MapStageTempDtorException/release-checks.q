#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# the IR of MapStageTempDtorException.qtest: the functional loops of values that cannot hold an object get no release
# check (a CheckException marked as a release check), and their containers no handler; the loops whose values can
# hold one get them (qore#6072)

%modern

class O {}
int sub inc(int i) { return i + 1; }
list<int> l = (1, 2, 3);
list<float> lf = (1.0, 2.0);
list<string> ls = ("a", "b");
list<list<int>> ll = ((1, 2), (3, 4));
int t = 0;
auto x;

try {
    # values that cannot hold an object
    t += (map ($1, inc($1))[1], l).size();
    t += (map $1 * 2, (select l, inc($1) > 2)).size();
    t += foldl $1 + inc($2), l;
    t += (map {"k" + $1: $1}, l).size();
    # results stored in a local, floats, strings, lists of ints; a map over an iterator object (pairIterator()), whose
    # elements are not typed, or with a method call resolved at run time keeps its checks, as the call can return an
    # object
    x = (map $1 + 1, l);
    x = (map $1 * 2.0, lf);
    x = (map $1 + "x", ls);
    t += foldl $1 + $2.size(), ll;
    x = (map $1[0], ll);
} catch () {
}
printf("int loops end\n");
try {
    # values that can hold an object
    x = (map (new O(), $1)[1], l);
} catch () {
}
printf("object loops end\n");
