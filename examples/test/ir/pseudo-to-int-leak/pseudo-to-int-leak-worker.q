#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# converts strings to integers outside the inline (48-bit) integer range, which are heap values in compiled code,
# with <string>::toInt() and strtoint(), and prints the results; run under a leak checker by pseudo-to-int-leak.qtest

%modern

sub take(auto v) {
}

#! the result is passed to a call
int sub passed(string s) {
    take(s.toInt());
    return 1;
}

#! the result is assigned to a typed local
int sub assigned(string s) {
    int i = s.toInt();
    take(i);
    return i;
}

#! the result is assigned to an untyped local that is never read
int sub unused(string s) {
    auto i = s.toInt();
    return 1;
}

#! the result is discarded
int sub discarded(string s) {
    s.toInt();
    return 1;
}

#! strtoint() raises an exception for a number out of range and returns a value with it, in a closure
auto sub in_closure(string s) {
    code c = auto sub () { return strtoint(s, 10); };
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# each function runs three times, called through a call reference so that each call is a call of the function itself:
# with native compilation following the first call of a function, the later calls run as native code with
# --exec-mode=jit or tiered
hash<string, string> inputs = {
    "passed": "9223372036854775807",
    "assigned": "-9223372036854775808",
    "unused": "140737488355328",
    "discarded": "9223372036854775807",
    "in_closure": "9223372036854775808",
};
hash<string, code<auto(string)>> calls = {
    "passed": \passed(),
    "assigned": \assigned(),
    "unused": \unused(),
    "discarded": \discarded(),
    "in_closure": \in_closure(),
};
list<auto> results;
for (int i = 0; i < 3; ++i) {
    foreach hash<auto> c in (calls.pairIterator()) {
        results += c.value(inputs{c.key});
    }
}
printf("%y\n", results);
