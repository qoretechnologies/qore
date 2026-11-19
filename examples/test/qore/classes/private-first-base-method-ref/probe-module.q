#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# probe of private-first-base-method-ref.qtest (qore#6680): call references to a method of a class of a compiled module
# (PrivateFirstBaseMod.qmod, which the test compiles with qcc -m), whose first parent class has a private method of
# the name

%modern
%requires PrivateFirstBaseMod

our PrivateFirstBaseMod::X lx = new PrivateFirstBaseMod::X();

string sub default_arg(code c = \lx.m()) {
    return c();
}

class H {
    public {
        code c = \lx.m();
    }
}

string sub try_call(code c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

sub main() {
    list<string> l = (
        # in the Program that imports the module: a call, a reference, a default argument value and a member's
        # initial value
        try_call(string sub () { return lx.m(); }),
        try_call(string sub () { code c = \lx.m(); return c(); }),
        try_call(string sub () { return default_arg(); }),
        try_call(string sub () { H h(); return h.c(); }),
        # in the module: a member's initial value and a default argument value
        try_call(string sub () { PrivateFirstBaseMod::Y y(); return y.c(); }),
        try_call(string sub () { return PrivateFirstBaseMod::Y::d(); }),
    );
    print(l.join("\n") + "\n");
}

main();
