#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the code of each pseudo-class refuses a value of another type than its class's with an exception, also when it is
# given one directly (dbg_call_pseudo_method(), debug builds only), and runs for a value of its type
# (pseudo-method-value-type.qtest, qore#7363); each line gives "<class> <method> <value>: <result or exception code>"

%modern

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

class AccessorObject {
}

AccessorObject obj();
# a value of the type of each pseudo-class whose code takes the value's node, and a method of the class
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
    # no method of <closure> reads its receiver: typeCode() gives the class's type for any value
    "<closure>": (sub () {}, "typeCode"),
};
# values of every type
hash<string, auto> values = {
    "string": "abc",
    "list": (1, 2),
    "hash": {"a": 1},
    "binary": <0102>,
    "date": 2026-01-01T00:00:00Z,
    "number": 1.5n,
    "buffer": new buffer<int64>((1, 2)),
    "object": obj,
    "callref": \result(),
    "closure": sub () {},
    "char": c'a',
    "int": 12345,
    "float": 2.5,
    "bool": True,
    "nothing": NOTHING,
    "null": NULL,
};
foreach hash<auto> c in (classes.pairIterator()) {
    foreach hash<auto> v in (values.pairIterator()) {
        auto class_value = c.value[0];
        string method = c.value[1];
        auto value = v.value;
        printf("%s %s %s: %s\n", c.key, method, v.key,
            result(auto sub () { return dbg_call_pseudo_method(class_value, method, value); }));
    }
}
