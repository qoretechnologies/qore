#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# containers whose value type is a date subtype, given containers of dates whose subtype is not known at parse time:
# each value is checked and the container takes the declared type; prints the outcome of each case

%modern

const AbsHash = {"a": 2026-01-01T00:00:00Z};
const RelHash = {"a": 1s};
const RelList = (1s,);

sub take_abs_hash(hash<string, date<absolute>> h) {
    printf("take_abs_hash: %y %s\n", h, h.fullType());
}

sub take_rel_list(list<date<relative>> l) {
    printf("take_rel_list: %y %s\n", l, l.fullType());
}

sub check(string name, code c) {
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", name, ex.err);
    }
}

# the value types of these containers are date, as at run time
hash<string, date> rel_hash = {"a": 1s};
hash<string, date> abs_hash = {"a": 2026-01-01T00:00:00Z};
list<date> rel_list = (1s, 2s);
list<date> abs_list = (2026-01-01T00:00:00Z,);

# a date of the other subtype is refused
check("constant hash", sub () { hash<string, date<absolute>> h = RelHash; printf("%y\n", h); });
check("constant list", sub () { list<date<absolute>> l = RelList; printf("%y\n", l); });
check("hash variable", sub () { hash<string, date<absolute>> h = rel_hash; printf("%y\n", h); });
check("list variable", sub () { list<date<relative>> l = abs_list; printf("%y\n", l); });
check("softlist variable", sub () { softlist<date<absolute>> l = rel_list; printf("%y\n", l); });
check("or nothing", sub () { hash<string, *date<relative>> h = abs_hash; printf("%y\n", h); });
check("nested", sub () {
    hash<string, list<date>> n = {"a": rel_list};
    hash<string, list<date<absolute>>> h = n;
    printf("%y\n", h);
});
check("hash argument", sub () { take_abs_hash(rel_hash); });
check("list argument", sub () { take_rel_list(abs_list); });
check("hash literal of a variable", sub () {
    date d = 1s;
    hash<string, date<absolute>> h = {"a": d};
    printf("%y\n", h);
});

# a date of the subtype is accepted, and the container takes the declared type
check("valid constant hash", sub () {
    hash<string, date<absolute>> h = AbsHash;
    printf("valid constant hash: %y %s\n", h, h.fullType());
});
check("valid hash variable", sub () {
    hash<string, date<absolute>> h = abs_hash;
    printf("valid hash variable: %y %s\n", h, h.fullType());
});
check("valid list variable", sub () {
    list<date<relative>> l = rel_list;
    printf("valid list variable: %y %s\n", l, l.fullType());
});
check("valid nested", sub () {
    hash<string, list<date>> n = {"a": abs_list};
    hash<string, list<date<absolute>>> h = n;
    printf("valid nested: %y %s\n", h, h.fullType());
});
check("valid hash argument", sub () { take_abs_hash(abs_hash); });
check("valid list argument", sub () { take_rel_list(rel_list); });
check("plain date", sub () {
    hash<string, date> h = rel_hash + abs_hash;
    printf("plain date: %y %s\n", h, h.fullType());
});
