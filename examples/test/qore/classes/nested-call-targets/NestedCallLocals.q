#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# static method calls and function calls nested 1 to 40 deep whose arguments read untyped locals, create references to
# them, take them in closures or assign them, at the innermost and at the outermost level (issue #6920): the analyses
# of the locals that the arguments of the calls use walk the expression of each call once; prints what each case
# computes and the values of its locals after it

%modern

class C {
    static auto id(auto x) {
        return x;
    }

    #! assigns v through r and returns v
    static auto set(reference<auto> r, auto v) {
        r = v;
        return v;
    }
}

auto sub id(auto x) {
    return x;
}

#! assigns v through r and returns v
auto sub set(reference<auto> r, auto v) {
    r = v;
    return v;
}

sub check(string name, code c) {
    printf("%s:\n", name);
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        printf("  %s: %s\n", ex.err, ex.desc);
    }
}

sub cases() {
    check("static method depth 1, locals read innermost", sub () { auto a = 1; auto b = "x"; printf("  %y\n", C::id(a + b)); });
    check("static method depth 1, reference innermost", sub () { auto v = 1; printf("  %y\n", C::id(C::set(\v, v + 1))); printf("  v: %y\n", v); });
    check("static method depth 1, reference outermost", sub () { auto v = 1; auto w = 10; printf("  %y\n", C::set(\v, C::id(v + w))); printf("  v: %y\n", v); });
    check("static method depth 1, closure innermost", sub () { auto v = 1; code c; printf("  %y\n", C::id((c = auto sub () { return v; })())); v = 2; printf("  c: %y\n", c()); });
    check("static method depth 1, assignment innermost", sub () { auto v = 1; printf("  %y\n", C::id(v = v + 1)); printf("  v: %y\n", v); });
    check("static method depth 4, locals read innermost", sub () { auto a = 1; auto b = "x"; printf("  %y\n", C::id(C::id(C::id(C::id(a + b))))); });
    check("static method depth 4, reference innermost", sub () { auto v = 1; printf("  %y\n", C::id(C::id(C::id(C::id(C::set(\v, v + 1)))))); printf("  v: %y\n", v); });
    check("static method depth 4, reference outermost", sub () { auto v = 1; auto w = 10; printf("  %y\n", C::set(\v, C::id(C::id(C::id(C::id(v + w)))))); printf("  v: %y\n", v); });
    check("static method depth 4, closure innermost", sub () { auto v = 1; code c; printf("  %y\n", C::id(C::id(C::id(C::id((c = auto sub () { return v; })()))))); v = 2; printf("  c: %y\n", c()); });
    check("static method depth 4, assignment innermost", sub () { auto v = 1; printf("  %y\n", C::id(C::id(C::id(C::id(v = v + 1))))); printf("  v: %y\n", v); });
    check("static method depth 5, locals read innermost", sub () { auto a = 1; auto b = "x"; printf("  %y\n", C::id(C::id(C::id(C::id(C::id(a + b)))))); });
    check("static method depth 5, reference innermost", sub () { auto v = 1; printf("  %y\n", C::id(C::id(C::id(C::id(C::id(C::set(\v, v + 1))))))); printf("  v: %y\n", v); });
    check("static method depth 5, reference outermost", sub () { auto v = 1; auto w = 10; printf("  %y\n", C::set(\v, C::id(C::id(C::id(C::id(C::id(v + w))))))); printf("  v: %y\n", v); });
    check("static method depth 5, closure innermost", sub () { auto v = 1; code c; printf("  %y\n", C::id(C::id(C::id(C::id(C::id((c = auto sub () { return v; })())))))); v = 2; printf("  c: %y\n", c()); });
    check("static method depth 5, assignment innermost", sub () { auto v = 1; printf("  %y\n", C::id(C::id(C::id(C::id(C::id(v = v + 1)))))); printf("  v: %y\n", v); });
    check("static method depth 8, locals read innermost", sub () { auto a = 1; auto b = "x"; printf("  %y\n", C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(a + b))))))))); });
    check("static method depth 8, reference innermost", sub () { auto v = 1; printf("  %y\n", C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::set(\v, v + 1)))))))))); printf("  v: %y\n", v); });
    check("static method depth 8, reference outermost", sub () { auto v = 1; auto w = 10; printf("  %y\n", C::set(\v, C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(v + w)))))))))); printf("  v: %y\n", v); });
    check("static method depth 8, closure innermost", sub () { auto v = 1; code c; printf("  %y\n", C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id((c = auto sub () { return v; })()))))))))); v = 2; printf("  c: %y\n", c()); });
    check("static method depth 8, assignment innermost", sub () { auto v = 1; printf("  %y\n", C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(v = v + 1))))))))); printf("  v: %y\n", v); });
    check("static method depth 40, locals read innermost", sub () { auto a = 1; auto b = "x"; printf("  %y\n", C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(a + b))))))))))))))))))))))))))))))))))))))))); });
    check("static method depth 40, reference innermost", sub () { auto v = 1; printf("  %y\n", C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::set(\v, v + 1)))))))))))))))))))))))))))))))))))))))))); printf("  v: %y\n", v); });
    check("static method depth 40, reference outermost", sub () { auto v = 1; auto w = 10; printf("  %y\n", C::set(\v, C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(v + w)))))))))))))))))))))))))))))))))))))))))); printf("  v: %y\n", v); });
    check("static method depth 40, closure innermost", sub () { auto v = 1; code c; printf("  %y\n", C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id((c = auto sub () { return v; })()))))))))))))))))))))))))))))))))))))))))); v = 2; printf("  c: %y\n", c()); });
    check("static method depth 40, assignment innermost", sub () { auto v = 1; printf("  %y\n", C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(C::id(v = v + 1))))))))))))))))))))))))))))))))))))))))); printf("  v: %y\n", v); });
    check("function depth 1, locals read innermost", sub () { auto a = 1; auto b = "x"; printf("  %y\n", id(a + b)); });
    check("function depth 1, reference innermost", sub () { auto v = 1; printf("  %y\n", id(set(\v, v + 1))); printf("  v: %y\n", v); });
    check("function depth 1, reference outermost", sub () { auto v = 1; auto w = 10; printf("  %y\n", set(\v, id(v + w))); printf("  v: %y\n", v); });
    check("function depth 1, closure innermost", sub () { auto v = 1; code c; printf("  %y\n", id((c = auto sub () { return v; })())); v = 2; printf("  c: %y\n", c()); });
    check("function depth 1, assignment innermost", sub () { auto v = 1; printf("  %y\n", id(v = v + 1)); printf("  v: %y\n", v); });
    check("function depth 4, locals read innermost", sub () { auto a = 1; auto b = "x"; printf("  %y\n", id(id(id(id(a + b))))); });
    check("function depth 4, reference innermost", sub () { auto v = 1; printf("  %y\n", id(id(id(id(set(\v, v + 1)))))); printf("  v: %y\n", v); });
    check("function depth 4, reference outermost", sub () { auto v = 1; auto w = 10; printf("  %y\n", set(\v, id(id(id(id(v + w)))))); printf("  v: %y\n", v); });
    check("function depth 4, closure innermost", sub () { auto v = 1; code c; printf("  %y\n", id(id(id(id((c = auto sub () { return v; })()))))); v = 2; printf("  c: %y\n", c()); });
    check("function depth 4, assignment innermost", sub () { auto v = 1; printf("  %y\n", id(id(id(id(v = v + 1))))); printf("  v: %y\n", v); });
    check("function depth 5, locals read innermost", sub () { auto a = 1; auto b = "x"; printf("  %y\n", id(id(id(id(id(a + b)))))); });
    check("function depth 5, reference innermost", sub () { auto v = 1; printf("  %y\n", id(id(id(id(id(set(\v, v + 1))))))); printf("  v: %y\n", v); });
    check("function depth 5, reference outermost", sub () { auto v = 1; auto w = 10; printf("  %y\n", set(\v, id(id(id(id(id(v + w))))))); printf("  v: %y\n", v); });
    check("function depth 5, closure innermost", sub () { auto v = 1; code c; printf("  %y\n", id(id(id(id(id((c = auto sub () { return v; })())))))); v = 2; printf("  c: %y\n", c()); });
    check("function depth 5, assignment innermost", sub () { auto v = 1; printf("  %y\n", id(id(id(id(id(v = v + 1)))))); printf("  v: %y\n", v); });
    check("function depth 8, locals read innermost", sub () { auto a = 1; auto b = "x"; printf("  %y\n", id(id(id(id(id(id(id(id(a + b))))))))); });
    check("function depth 8, reference innermost", sub () { auto v = 1; printf("  %y\n", id(id(id(id(id(id(id(id(set(\v, v + 1)))))))))); printf("  v: %y\n", v); });
    check("function depth 8, reference outermost", sub () { auto v = 1; auto w = 10; printf("  %y\n", set(\v, id(id(id(id(id(id(id(id(v + w)))))))))); printf("  v: %y\n", v); });
    check("function depth 8, closure innermost", sub () { auto v = 1; code c; printf("  %y\n", id(id(id(id(id(id(id(id((c = auto sub () { return v; })()))))))))); v = 2; printf("  c: %y\n", c()); });
    check("function depth 8, assignment innermost", sub () { auto v = 1; printf("  %y\n", id(id(id(id(id(id(id(id(v = v + 1))))))))); printf("  v: %y\n", v); });
    check("function depth 40, locals read innermost", sub () { auto a = 1; auto b = "x"; printf("  %y\n", id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(a + b))))))))))))))))))))))))))))))))))))))))); });
    check("function depth 40, reference innermost", sub () { auto v = 1; printf("  %y\n", id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(set(\v, v + 1)))))))))))))))))))))))))))))))))))))))))); printf("  v: %y\n", v); });
    check("function depth 40, reference outermost", sub () { auto v = 1; auto w = 10; printf("  %y\n", set(\v, id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(v + w)))))))))))))))))))))))))))))))))))))))))); printf("  v: %y\n", v); });
    check("function depth 40, closure innermost", sub () { auto v = 1; code c; printf("  %y\n", id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id((c = auto sub () { return v; })()))))))))))))))))))))))))))))))))))))))))); v = 2; printf("  c: %y\n", c()); });
    check("function depth 40, assignment innermost", sub () { auto v = 1; printf("  %y\n", id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(id(v = v + 1))))))))))))))))))))))))))))))))))))))))); printf("  v: %y\n", v); });
}

# the cases run more than once, so that the tiered execution mode compiles them
for (int i = 0; i < 3; ++i) {
    printf("round %d\n", i);
    cases();
}
