#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

%modern
%enable-all-warnings

# Regression fixture for issue #7363: a pseudo-method that the parser resolved for the declared type of an expression
# runs on a value of another type.  Each case calls through a call reference whose declared return type the function
# behind it does not honor, so the parser resolves the method of the declared type and the value has another one.

auto sub get_int() {
    return 1234567;
}

auto sub get_list() {
    return (1, 2, 3);
}

auto sub get_hash() {
    return {"a": 1, "b": 2};
}

auto sub get_string() {
    return "hello";
}

auto sub get_nothing() {
}

code<string()> as_string_int = \get_int();
code<string()> as_string_list = \get_list();
code<string()> as_string_nothing = \get_nothing();
code<string()> as_string = \get_string();
code<list<auto>()> as_list_int = \get_int();
code<list<auto>()> as_list_hash = \get_hash();
code<list<auto>()> as_list_string = \get_string();
code<hash<auto>()> as_hash_int = \get_int();
code<hash<auto>()> as_hash_string = \get_string();
code<int()> as_int_string = \get_string();
code<int()> as_int = \get_int();

# the method does not exist for the type of the value
auto sub c1() { return as_string_int().strlen(); }
auto sub c2() { return as_string_int().upr(); }
auto sub c3() { return as_list_int().first(); }
auto sub c4() { return as_hash_int().keys(); }
auto sub c5() { return as_int_string().abs(); }
auto sub c6() { return as_hash_string().keys(); }
auto sub c7() { return as_string_nothing().strlen(); }

# the method of the name exists for the type of the value: that one runs, not the one for the declared type
auto sub d1() { return as_string_list().size(); }
auto sub d2() { return as_list_string().size(); }
auto sub d3() { return as_list_hash().size(); }
auto sub d4() { return as_string_int().typeCode(); }

# the value is of the declared type
auto sub e1() { return as_string().strlen(); }
auto sub e2() { return as_string().upr(); }
auto sub e3() { return as_int().abs(); }

sub run(string label, code c) {
    try {
        printf("%s: %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: EXCEPTION %s\n", label, ex.err);
    }
}

run("string::strlen on int", \c1());
run("string::upr on int", \c2());
run("list::first on int", \c3());
run("hash::keys on int", \c4());
run("int::abs on string", \c5());
run("hash::keys on string", \c6());
run("string::strlen on nothing", \c7());
run("size of a list as string", \d1());
run("size of a string as list", \d2());
run("size of a hash as list", \d3());
run("typeCode of an int as string", \d4());
run("strlen of a string", \e1());
run("upr of a string", \e2());
run("abs of an int", \e3());
