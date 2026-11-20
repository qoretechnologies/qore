#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The cases of parse-time-method-wrong-type.qtest (qore#6813), for debug builds of the library: a method call resolved
# at parse time from the declared type of a builtin's value (dbg_value_as_socket(), declared to return Socket) on a
# value of another type behaves as the same call on a value with no declared type.  Each line gives the result or the
# exception of a call, as "<typed|untyped> <method> <value type>: <result>".

%modern

auto sub id(auto v) {
    return v;
}

string sub result(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

hash<string, auto> vals = {
    "int": 1,
    "string": "abc",
    "nothing": NOTHING,
    "hash": {"a": 1},
    "list": (1, 2),
    "float": 1.5,
};

foreach hash<auto> i in (vals.pairIterator()) {
    auto v = i.value;
    printf("typed isOpen %s: %s\n", i.key, result(auto sub () { return dbg_value_as_socket(v).isOpen(); }));
    printf("typed size %s: %s\n", i.key, result(auto sub () { return dbg_value_as_socket(v).size(); }));
    printf("typed toString %s: %s\n", i.key, result(auto sub () { return dbg_value_as_socket(v).toString(); }));
    printf("typed className %s: %s\n", i.key, result(auto sub () { return dbg_value_as_socket(v).className(); }));
    auto u = id(v);
    printf("untyped isOpen %s: %s\n", i.key, result(auto sub () { return u.isOpen(); }));
    printf("untyped size %s: %s\n", i.key, result(auto sub () { return u.size(); }));
    printf("untyped toString %s: %s\n", i.key, result(auto sub () { return u.toString(); }));
    printf("untyped className %s: %s\n", i.key, result(auto sub () { return u.className(); }));
}
