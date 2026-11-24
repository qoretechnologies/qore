#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a pseudo-method called through the reflection API takes a value of its class's type and refuses one of another
# type (pseudo-method-value-type.qtest, qore#7363); each line gives "<class> <method> <value>: <result or exception
# code>"

%modern

%requires reflection

string sub result(code c) {
    try {
        auto rv = c();
        # a unique hash of a call reference differs between runs
        if (rv.typeCode() == NT_STRING && rv =~ /^[0-9a-f]{40}$/) {
            return "<unique hash>";
        }
        return regex_subst(sprintf("%y", rv), "0x[0-9a-f]+", "0x", RE_Global);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

class ReflectionObject {
}

ReflectionObject obj();
# a value of the type of each pseudo-class, and a method of the class that reads its value
hash<string, list<auto>> classes = {
    "<string>": ("abc", "size"),
    "<list>": ((1, 2), "size"),
    "<hash>": ({"a": 1}, "firstKey"),
    "<binary>": (<0102>, "size"),
    "<date>": (2026-01-01T00:00:00Z, "years"),
    "<number>": (1.5n, "sign"),
    "<buffer>": (new buffer<int64>((1, 2)), "size"),
    "<object>": (obj, "keys"),
    "<callref>": (\result(), "uniqueHash"),
    "<char>": (c'a', "ord"),
    "<int>": (12345, "abs"),
};
# values of every type
hash<string, auto> values = {
    "string": "abc",
    "long string": "a string that is longer than a short string",
    "list": (1, 2),
    "hash": {"a": 1},
    "binary": <0102>,
    "date": 2026-01-01T00:00:00Z,
    "number": 1.5n,
    "buffer": new buffer<int64>((1, 2)),
    "object": obj,
    "callref": \result(),
    "char": c'a',
    "int": 12345,
    "nothing": NOTHING,
};
foreach hash<auto> c in (classes.pairIterator()) {
    PseudoClass pc(c.value[0]);
    PseudoMethod m = pc.getMethod(c.value[1]);
    foreach hash<auto> v in (values.pairIterator()) {
        auto value = v.value;
        printf("%s %s %s: %s\n", c.key, c.value[1], v.key, result(auto sub () { return m.call(value); }));
    }
}
