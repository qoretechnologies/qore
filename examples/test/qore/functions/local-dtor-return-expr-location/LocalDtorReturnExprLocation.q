#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

%modern

# Fixture for issue #7481: the destructor of a local released after a return expression that calls a method of the local
# runs in the location of the cleanup of the local's block, in every execution mode.  The output records the line that
# the frame of each destructor (and the location of each exception that a destructor raises) names; the lines of this
# file are part of the expected output (LocalDtorReturnExprLocation.expected).

class Dtor inherits Mutex {
    private { string label; }
    constructor(string n_label) { label = n_label; }
    destructor() {
        printf("  destructor %s at line %d\n", label, get_thread_call_stack()[1].line);
    }
    bool owner() { return lockOwner(); }
}

class Holder {
    auto viaMethod() {
        Dtor m("method local");
        return m.lockOwner();
    }
}

sub show(string name, code c) {
    printf("%s\n", name);
    try {
        printf("  result %y\n", c());
    } catch (hash<ExceptionInfo> ex) {
        printf("  exception %s at line %d\n", ex.err, ex.line);
    }
}

auto sub plain() {
    Dtor m("plain");
    return 1;
}

auto sub stored() {
    Dtor m("stored");
    auto x = m.lockOwner();
    return x;
}

auto sub call() {
    Dtor m("call");
    return m.lockOwner();
}

auto sub call_member() {
    Dtor m("member");
    return m.owner();
}

auto sub nested(bool b) {
    if (b) {
        Dtor m("nested");
        return m.lockOwner();
    }
    return 0;
}

auto sub two() {
    Dtor a("first");
    Dtor b("second");
    return b.lockOwner();
}

auto sub in_expression() {
    Dtor m("expression");
    return (m.lockOwner(), 2)[1];
}

auto sub locked() {
    Dtor m("locked");
    return m.trylock();
}

show("closure", auto sub () { Dtor m("closure"); return m.lockOwner(); });
show("plain", \plain());
show("stored", \stored());
show("function", \call());
show("member of the class", \call_member());
show("nested block", auto sub () { return nested(True); });
show("two locals", \two());
show("in an expression", \in_expression());
show("method", auto sub () { Holder h(); return h.viaMethod(); });
show("destructor exception", \locked());
printf("done\n");
