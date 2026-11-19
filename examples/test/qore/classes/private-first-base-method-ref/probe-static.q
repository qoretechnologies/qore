#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# probe of private-first-base-method-ref.qtest (qore#6680): a reference to a static method resolves the method that a
# call of it resolves, where a class inherits the name from two public parent classes and the first one's method is
# private; qcc executables store such a reference with the class it was written with and are checked once qore#6679
# is fixed

%modern

class A {
    private static string s() { return "A::s"; }
}

class B {
    static string s() { return "B::s"; }
}

class X inherits A, B {
}

string sub try_call(code c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

sub main() {
    X x();
    list<string> l = (
        try_call(string sub () { return X::s(); }),
        try_call(string sub () { code c = \X::s(); return c(); }),
        try_call(string sub () { return x.s(); }),
        try_call(string sub () { code c = \x.s(); return c(); }),
    );
    print(l.join("\n") + "\n");
}

main();
