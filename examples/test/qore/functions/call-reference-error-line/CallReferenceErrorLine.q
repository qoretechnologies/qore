#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a call of a value that is not a call reference raises REFERENCE-CALL-ERROR with the line of the call: when its
# arguments can have side effects, by the check of its target before they are evaluated, and otherwise by the call
# (issue #5835); prints the error and its line for each case

%modern

auto sub one() {
    return 1;
}

#! an argument of type auto, which can hold a reference
sub auto_local_arg(auto v) {
    auto i = 1;
    v(i);
}

#! a function call argument
sub call_arg(auto v) {
    v(one());
}

#! a reference parameter as the argument
sub reference_arg(auto v, reference<int> r) {
    v(r);
}

#! a literal argument
sub literal(auto v) {
    v(1);
}

#! a local argument
sub local_arg(auto v) {
    int i = 1;
    v(i);
}

#! no argument
sub no_arg(auto v) {
    v();
}

#! parameters as arguments
sub param_args(auto v, int i) {
    v(i, i);
}

#! a value of a local
sub local_value() {
    auto v = {};
    int i = 1;
    v(i);
}

class K {
    #! in a method, with a literal argument
    m(auto v) {
        v(1);
    }

    #! in a method, with a parameter argument
    n(auto v, int i) {
        v(i);
    }
}

sub check(string name, code c) {
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s line %d: %s\n", name, ex.err, ex.line, ex.desc);
    }
}

# the cases run more than once, so that the tiered execution mode compiles them
for (int r = 0; r < 3; ++r) {
    check("literal argument", sub () { literal("x"); });
    check("local argument", sub () { local_arg({}); });
    check("no argument", sub () { no_arg(1); });
    check("parameter arguments", sub () { param_args(1.5, 2); });
    check("value of a local", \local_value());
    check("method, literal argument", sub () { K k(); k.m("y"); });
    check("method, parameter argument", sub () { K k(); k.n(True, 3); });
    # arguments with side effects: the target is checked before they are evaluated
    check("auto local argument", sub () { auto_local_arg("x"); });
    check("function call argument", sub () { call_arg({}); });
    check("reference parameter argument", sub () { int x = 1; reference_arg(1, \x); });
    # a direct call of the function, not through a closure
    try {
        local_arg(2);
    } catch (hash<ExceptionInfo> ex) {
        printf("direct call: %s line %d\n", ex.err, ex.line);
    }
}
