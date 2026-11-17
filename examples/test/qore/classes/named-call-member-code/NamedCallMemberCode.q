#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# A call of a method that a class does not have, of the name of a member that holds code, calls the member's code;
# a named call binds its names to the parameters of that code (qore#6361), whether the member is untyped (auto) or
# of a code type, and a positional call is not checked against the pseudo-method of the same name.  A member of the
# name that does not hold code leaves the call to the pseudo-method, with the names bound to its parameters.  Prints
# the result or the exception code of each call.

%modern

class A {
    public {
        # the name of a pseudo-method of objects (<object>::hasCallableMethod(string method))
        auto hasCallableMethod;
        auto f;
        code cm;
        *code ocm;
    }

    string meth() {
        return "meth";
    }
}

string sub show(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

sub test() {
    A a();
    a.hasCallableMethod = auto sub (*string m, *string other) { return sprintf("hcm(%y, %y)", m, other); };
    a.f = auto sub (string x, int n = 1) { return sprintf("f(%y, %d)", x, n); };
    a.cm = auto sub (*string m, *string other) { return sprintf("cm(%y, %y)", m, other); };
    a.ocm = auto sub (string first, string second) { return first + "-" + second; };

    printf("auto, a name of the code: %s\n", show(auto sub () { return a.hasCallableMethod(other: "x"); }));
    printf("auto, both names: %s\n", show(auto sub () { return a.hasCallableMethod(other: "o", m: "m"); }));
    printf("auto, positional: %s\n", show(auto sub () { return a.hasCallableMethod("x", "y"); }));
    printf("auto, the pseudo-method's name: %s\n", show(auto sub () { return a.hasCallableMethod(method: "x"); }));
    printf("auto, default: %s\n", show(auto sub () { return a.f(x: "v"); }));
    printf("auto, mixed: %s\n", show(auto sub () { return a.f("v", n: 3); }));
    printf("code: %s\n", show(auto sub () { return a.cm(other: "x"); }));
    printf("*code: %s\n", show(auto sub () { return a.ocm(second: "2", first: "1"); }));

    # a member that does not hold code: the pseudo-method, with the names bound to its parameters
    a.hasCallableMethod = "not code";
    printf("not code, named: %s\n", show(auto sub () { return a.hasCallableMethod(method: "meth"); }));
    printf("not code, positional: %s\n", show(auto sub () { return a.hasCallableMethod("nonexistent"); }));
}

test();
