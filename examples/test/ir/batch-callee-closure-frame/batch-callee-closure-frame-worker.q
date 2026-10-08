#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# calls recursive functions with locals captured by closures from functions that compile them in their native
# batch, and prints the results

%modern

#! a recursive function whose local is captured by a closure; returns the closure results of all levels
list<int> sub captured_list(int depth) {
    int value = depth;
    code<int()> increment = int sub () { return ++value; };
    list<int> result();
    if (depth) {
        result = captured_list(depth - 1);
    }
    push result, increment();
    return result;
}

#! a recursive function whose local is captured by a closure; returns the sum of the closure results of all levels
int sub captured_sum(int depth) {
    int value = depth;
    code<int()> increment = int sub () { return ++value; };
    int rv = 0;
    if (depth) {
        rv = captured_sum(depth - 1);
    }
    return rv + increment();
}

#! calls the recursive functions; compiles them as callees in its batch
string sub caller() {
    return sprintf("%y %d", captured_list(3), captured_sum(2));
}

#! calls the recursive functions too; compiles them as callees in its own batch as well
string sub other_caller() {
    return sprintf("%y", captured_list(1));
}

printf("%s\n", caller());
printf("%s\n", other_caller());
printf("%s\n", caller());
