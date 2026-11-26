#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a function whose return value fails its declared return type, called by compiled code through the runtime's fast
# call of a function: the exception is raised with the function on the call stack, as the AST interpreter raises it
# in the call's frame; prints the exception and the call stack of the user functions

%modern
%requires ./FastCallReturnCheckModule.qm

int sub local_checked(string s) {
    auto v = s == "ok" ? 1 : s;
    return v;
}

int sub call_module(string s) {
    return mod_checked(s);
}

int sub call_local(string s) {
    return local_checked(s);
}

#! an object whose destructor raises an exception
class Tmp {
    private {
        string n;
        bool raise;
    }

    constructor(string n, bool raise) {
        self.n = n;
        self.raise = raise;
    }

    destructor() {
        if (raise) {
            throw "DESTRUCTOR-ERROR", n;
        }
    }

    string name() {
        return n;
    }
}

int sub given_checked(Tmp t) {
    auto v = t.name() == "ok" ? 1 : t.name();
    return v;
}

# the argument is a temporary, whose reference the call gives to the callee (issue #5882): the return value is checked
# before the parameter, whose destructor raises, is released
int sub call_given(string s, bool raise) {
    return given_checked(new Tmp(s, raise));
}

string sub errs(hash<ExceptionInfo> ex) {
    list<string> l = ();
    *hash<ExceptionInfo> e = ex;
    while (e) {
        push l, e.err;
        e = e.next;
    }
    return l.join(",");
}

string sub stack_of(hash<ExceptionInfo> ex) {
    return (map sprintf("%s:%d", $1.function, $1.line), ex.callstack, $1.type == "user").join(" <- ");
}

int t = 0;
for (int i = 0; i < 300; ++i) {
    t += call_module("ok") + call_local("ok") + call_given("ok", False);
}
printf("warm-up %d\n", t);
foreach string name in (("module", "local", "given")) {
    try {
        switch (name) {
            case "module": call_module("bad"); break;
            case "local": call_local("bad"); break;
            case "given": call_given("bad", True); break;
        }
        printf("%s: no exception\n", name);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s; stack: %s; exceptions: %s\n", name, ex.err, ex.desc, stack_of(ex), errs(ex));
    }
}
