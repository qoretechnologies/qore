#!/usr/bin/env qore
# Copyright 2026 Qore Technologies, s.r.o.
# Calls date<absolute>/date<relative> parameters with date-typed arguments of the wrong kind; each call must raise
# RUNTIME-TYPE-ERROR as in AST, also when the compiled call goes directly to the callee's native entry (#5730)

%modern
%exec-class AOTDateSubtypeParamsProbe

sub fa(date<absolute> d) { printf("fa %y\n", d); }
sub fr(date<relative> d) { printf("fr %y\n", d); }
sub fan(*date<absolute> d) { printf("fan %y\n", d); }
sub frn(*date<relative> d) { printf("frn %y\n", d); }
sub fa2(int i, date<absolute> d) { printf("fa2 %d %y\n", i, d); }
sub fhot(date<absolute> d) { }

hashdecl AbsHash {
    date<absolute> a;
}

class C {
    ma(date<absolute> d) { printf("ma %y\n", d); }
    static sa(date<absolute> d) { printf("sa %y\n", d); }
    static shot(date<absolute> d) { }
}

class AOTDateSubtypeParamsProbe {
    constructor() {
        date drel = 2s;
        date dabs = 2026-01-02T03:04:05Z;
        *date ndrel = 3s;
        *date ndabs = 2026-01-03T00:00:00Z;
        *date nothing_date;

        # each case is called once with a value of the right kind and once with one of the wrong kind
        list<hash<auto>> cases = (
            {"name": "fa(date)", "ok": sub () { fa(dabs); }, "bad": sub () { fa(drel); }},
            {"name": "fa(literal)", "ok": sub () { fa(2026-01-01T00:00:00Z); }, "bad": sub () { fa(1s); }},
            {"name": "fr(date)", "ok": sub () { fr(drel); }, "bad": sub () { fr(dabs); }},
            {"name": "fan(*date)", "ok": sub () { fan(ndabs); fan(nothing_date); }, "bad": sub () { fan(ndrel); }},
            {"name": "fan(date)", "ok": sub () { fan(dabs); }, "bad": sub () { fan(drel); }},
            {"name": "frn(*date)", "ok": sub () { frn(ndrel); }, "bad": sub () { frn(ndabs); }},
            {"name": "fa2(int, date)", "ok": sub () { fa2(1, dabs); }, "bad": sub () { fa2(1, drel); }},
            {"name": "C::sa(date)", "ok": sub () { C::sa(dabs); }, "bad": sub () { C::sa(drel); }},
            {"name": "c.ma(date)", "ok": sub () { new C().ma(dabs); }, "bad": sub () { new C().ma(drel); }},
            {"name": "hashdecl member", "ok": sub () { hash<AbsHash> h = <AbsHash>{"a": dabs}; printf("h %y\n", h.a); },
                "bad": sub () { hash<AbsHash> h = <AbsHash>{"a": drel}; printf("h %y\n", h.a); }},
            {"name": "closure", "ok": sub () { code c = sub (date<absolute> d) { printf("cl %y\n", d); }; c(dabs); },
                "bad": sub () { code c = sub (date<absolute> d) { printf("cl %y\n", d); }; c(drel); }},
        );
        foreach hash<auto> c in (cases) {
            c.ok();
            try {
                c.bad();
                printf("%s: no error\n", c.name);
            } catch (hash<ExceptionInfo> ex) {
                printf("%s: %s: %s\n", c.name, ex.err, ex.desc);
            }
        }

        # hot callees: called often enough with valid arguments to run natively before the invalid call
        list<hash<auto>> hot = (
            {"name": "hot fhot(date)", "call": sub (date d) { fhot(d); }},
            {"name": "hot C::shot(date)", "call": sub (date d) { C::shot(d); }},
        );
        foreach hash<auto> h in (hot) {
            for (int i = 0; i < 50; ++i) {
                h.call(dabs);
            }
            try {
                h.call(drel);
                printf("%s: no error\n", h.name);
            } catch (hash<ExceptionInfo> ex) {
                printf("%s: %s\n", h.name, ex.err);
            }
        }
    }
}
