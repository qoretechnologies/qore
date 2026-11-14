#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# the probe of AOTGenericLocalType.qtest (qore#5885): locals of generic functions and methods whose types name the
# type parameters (union<T, int>, *T) refuse the values that the instantiated type refuses, in a function's body, in
# a block, in an on_exit block and in a closure, for each instantiation; its output in every execution mode and as a
# qcc executable must be that of the AST interpreter

%modern

int sub unionLocal<T>(T x, auto y) {
    union<T, int> z = x;
    z = y;
    printf("union z=%y\n", z);
    return 1;
}

int sub optLocal<T>(T x, auto y) {
    *T z = x;
    z = y;
    printf("opt z=%y\n", z);
    return 1;
}

int sub blockLocal<T>(T x, auto y) {
    if (x) {
        *T z = x;
        z = y;
        printf("block z=%y\n", z);
    }
    return 1;
}

int sub onExitLocal<T>(T x, auto y) {
    {
        on_exit {
            *T z = x;
            z = y;
            printf("on_exit z=%y\n", z);
        }
    }
    return 1;
}

int sub closureLocal<T>(T x, auto y) {
    code c = sub () {
        *T z = x;
        z = y;
        printf("closure z=%y\n", z);
    };
    c();
    return 1;
}

int sub onSuccessLocal<T>(T x, auto y) {
    {
        on_success {
            *T z = x;
            z = y;
            printf("on_success z=%y\n", z);
        }
    }
    return 1;
}

int sub onErrorLocal<T>(T x, auto y) {
    {
        on_error {
            *T z = x;
            z = y;
            printf("on_error z=%y\n", z);
        }
        throw "ERR", "error";
    }
}

class GenericLocalHolder {
    int keep<T>(T x, auto y) {
        *T z = x;
        z = y;
        printf("method z=%y\n", z);
        return 1;
    }
}

sub show(string label, code c) {
    try {
        printf("%s => %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s => %s: %s\n", label, ex.err, ex.desc);
    }
}

show("union<string, int> float", auto sub () { return unionLocal<string>("a", 2.5); });
show("union<string, int> int", auto sub () { return unionLocal<string>("a", 3); });
show("union<int, int> string", auto sub () { return unionLocal<int>(1, "s"); });
show("*string float", auto sub () { return optLocal<string>("a", 2.5); });
show("*string string", auto sub () { return optLocal<string>("a", "b"); });
show("*int string", auto sub () { return optLocal<int>(1, "s"); });
show("*int int", auto sub () { return optLocal<int>(1, 2); });
show("block *string float", auto sub () { return blockLocal<string>("a", 2.5); });
show("block *string string", auto sub () { return blockLocal<string>("a", "b"); });
show("on_exit *string float", auto sub () { return onExitLocal<string>("a", 2.5); });
show("closure *string float", auto sub () { return closureLocal<string>("a", 2.5); });
show("closure *string string", auto sub () { return closureLocal<string>("a", "b"); });
show("on_exit *string int", auto sub () { return onExitLocal<string>("a", 2); });
show("on_exit *string bool", auto sub () { return onExitLocal<string>("a", True); });
show("on_exit *string string", auto sub () { return onExitLocal<string>("a", "b"); });
show("on_exit *int float", auto sub () { return onExitLocal<int>(1, 2.5); });
show("on_exit *int int", auto sub () { return onExitLocal<int>(1, 2); });
show("on_success *string float", auto sub () { return onSuccessLocal<string>("a", 2.5); });
show("on_error *string float", auto sub () { return onErrorLocal<string>("a", 2.5); });
show("on_error *string string", auto sub () { return onErrorLocal<string>("a", "b"); });
GenericLocalHolder h();
show("method *string float", auto sub () { return h.keep<string>("a", 2.5); });
show("method *string string", auto sub () { return h.keep<string>("a", "b"); });
