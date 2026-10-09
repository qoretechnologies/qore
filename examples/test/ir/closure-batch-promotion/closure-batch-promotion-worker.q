#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs closures that are compiled to native code in the batch of the function that creates them and are then called
# by builtin code often enough to be promoted to native code themselves; prints the results

%modern

#! creates a closure and calls it with call_function() and map
int sub scale(int k) {
    code<int(int)> c = int sub (int i) { return i * k; };
    list<int> l = map c($1), (1, 2, 3);
    return (foldl $1 + $2, l) + call_function(c, 5);
}

#! creates a closure and passes it to sort() as the comparator
list<int> sub sorted(int dir) {
    return sort((3, 1, 2), int sub (int a, int b) { return (a <=> b) * dir; });
}

# the functions are called through call references, so each call is a call of the function itself, and often enough to
# promote them and then their closures with --exec-mode=jit and tiered
code<int(int)> s = \scale();
code<list<int>(int)> o = \sorted();
list<auto> results;
for (int i = 0; i < 4; ++i) {
    results += (s(2), o(1), o(-1));
}
printf("%y\n", results);
