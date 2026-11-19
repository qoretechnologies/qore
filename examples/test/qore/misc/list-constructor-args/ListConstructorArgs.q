#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (C) 2026 Qore Technologies, s.r.o.

# the arguments of the constructor of a typed list are its elements: an argument that is not a valid element raises
# the same error at parse time and at run time (issue #6172); its output is ListConstructorArgs.expected in every
# execution mode and in programs compiled with qcc

%modern

hashdecl P {
    int a;
}

sub check(string name, code c) {
    try {
        auto v = c();
        printf("%s: %y %s\n", name, v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", name, ex.err, ex.desc);
    }
}

#! parses code in a Program of its own and reports the parse error
sub check_parse(string name, string code) {
    Program pgm(PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    try {
        pgm.parse("hashdecl P { int a; } " + code, name);
        printf("%s: parsed\n", name);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", name, ex.err, ex.desc);
    }
}

# errors raised when the code is parsed
check_parse("parse: a list as the argument", "list<hash<P>> l((cast<hash<P>>({\"a\": 1}),));");
check_parse("parse: an argument of another type", "list<int> l(1, \"x\");");
check_parse("parse: new with a list as the argument", "list<int> l = new list<int>((1, 2));");

# the cases run more than once, so that the tiered execution mode compiles them
for (int i = 0; i < 3; ++i) {
    printf("round %d\n", i);
    # errors raised at run time, for arguments whose types are only known then
    check("run time: a list as the argument", auto sub () {
        auto a = (cast<hash<P>>({"a": 1}),);
        list<hash<P>> l(a);
        return l;
    });
    check("run time: an argument of another type", auto sub () {
        auto s = "x";
        list<int> l(1, s);
        return l;
    });
    check("run time: new with a list as the argument", auto sub () {
        auto a = (1, 2);
        return new list<int>(a);
    });
    # the arguments are the elements of the list
    check("elements as the arguments", auto sub () {
        list<hash<P>> l(cast<hash<P>>({"a": 1}), cast<hash<P>>({"a": 2}));
        return l;
    });
    check("one element", auto sub () {
        list<hash<P>> l(cast<hash<P>>({"a": 1}));
        return l;
    });
    check("assignment of a list", auto sub () {
        list<hash<P>> l = (cast<hash<P>>({"a": 1}),);
        return l;
    });
    check("elements known at run time", auto sub () {
        auto a = 3;
        list<int> l(a, 4);
        return l;
    });
}
