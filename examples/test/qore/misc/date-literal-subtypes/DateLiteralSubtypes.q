#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# date literals used where their subtype fits, and types inferred from them: none of it is a parse error, and every
# execution mode prints the values that the AST interpreter prints

%modern

sub take_abs(date<absolute> d) {
    printf("take_abs: %y\n", d);
}

sub take_rel(date<relative> d) {
    printf("take_rel: %y\n", d);
}

sub take_date(date d) {
    printf("take_date: %y\n", d);
}

sub take_abs_or_nothing(*date<absolute> d) {
    printf("take_abs_or_nothing: %y\n", d);
}

sub take_rel_or_nothing(*date<relative> d) {
    printf("take_rel_or_nothing: %y\n", d);
}

date<absolute> sub ret_abs() {
    return 2026-01-02T03:04:05Z;
}

date<relative> sub ret_rel() {
    return 2D;
}

class C {
    static sm(date<relative> d) {
        printf("C::sm: %y\n", d);
    }

    m(date<absolute> d) {
        printf("C::m: %y\n", d);
    }
}

const Rel = 1h;
const Abs = 2026-03-04T00:00:00Z;

# each literal where its subtype is required, where a date is required, and where NOTHING is also accepted
take_abs(2026-01-01T00:00:00Z);
take_rel(1s);
take_rel(P1DT2H);
take_date(1s);
take_date(2026-01-01T00:00:00Z);
take_abs_or_nothing(2026-01-01T00:00:00Z);
take_rel_or_nothing(3m);
printf("ret: %y %y\n", ret_abs(), ret_rel());
C::sm(5s);
C c();
c.m(2026-02-01T00:00:00Z);
take_rel(Rel);
take_abs(Abs);
date<relative> r = 1s;
date<absolute> a = 2026-01-01T00:00:00Z;
*date<relative> rn = 2s;
*date<absolute> an = 2026-01-02T00:00:00Z;
printf("typed: %y %y %y %y\n", r, a, rn, an);

# containers built from literals of both subtypes hold dates: values of either subtype can be added
auto h = {"a": 1s};
h.b = 2026-01-01T00:00:00Z;
auto l = (1s, 2s);
push l, 2026-01-01T00:00:00Z;
list<date> ld = (1s, 2026-01-01T00:00:00Z);
hash<string, date> hd = {"a": 1s, "b": 2026-01-01T00:00:00Z};
auto hm = {"a": 2026-01-01T00:00:00Z, "b": 1s};
hm.c = 4s;
auto m = map 1s, (1, 2);
push m, 2026-01-01T00:00:00Z;
auto hmap = map {$1: 1s}, ("a", "b");
hmap.c = 2026-01-01T00:00:00Z;
list<auto> mixed = (r, a, 1s, 2026-01-01T00:00:00Z);
printf("containers: %y %y %y %y %y %y %y %y\n", h, l, ld, hd, hm, m, hmap, mixed);

# a ternary of both subtypes is a date
auto t = (ld.size() > 1) ? 1s : 2026-01-01T00:00:00Z;
t = 2026-05-06T00:00:00Z;
take_abs(t);

# a local assigned a literal keeps the type of a date: it can take a date of the other subtype later
auto x = 1s;
x = 2026-01-01T00:00:00Z;
take_abs(x);
auto y = 2026-01-01T00:00:00Z;
y += 1D;
take_abs(y);
auto z = r;
z = a;
take_abs(z);
