#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# variants that differ only in the signature of a typed callable parameter are different variants, and the signature
# of the callable selects one at parse time and at run time (qore#6660)

%modern

string sub ov(code<int(int)> c) { return "int(int)"; }
string sub ov(code<string()> c) { return "string()"; }

auto sub get_int() { return int sub (int x) { return x; }; }
auto sub get_str() { return string sub () { return "x"; }; }
auto sub get_date() { return date sub () { return now(); }; }

printf("parse time: %s %s\n", ov(int sub (int x) { return x; }), ov(string sub () { return "x"; }));
printf("run time: %s %s\n", ov(get_int()), ov(get_str()));
try {
    ov(get_date());
} catch (hash<ExceptionInfo> ex) {
    printf("no variant: %s\n", ex.err);
}
