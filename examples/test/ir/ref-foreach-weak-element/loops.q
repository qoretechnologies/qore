#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# The loops of ref-foreach-weak-element.qtest: a foreach over a list by reference whose elements are weak references
# (:=) to objects whose only strong reference is in g_h; the destructors log where each object is released, and the
# loops log whether an element still holds an object afterwards (qore#6633)

%modern
%allow-weak-references

string log;

class T {
    public { string n; }
    constructor(string n) { self.n = n; }
    destructor() { log += "~" + n; }
    clear() { remove g_h{n}; log += "|c" + n; }
}

our hash<string, T> g_h;

sub put(string n) { g_h{n} = new T(n); }

sub report(string name) { printf("%s: %s\n", name, log); log = ""; }

# the last strong reference is removed in the body
sub noCall() {
    put("a");
    list<auto> l = ();
    l[0] := g_h.a;
    foreach T w in (\l) {
        remove g_h.a;
        log += "|s";
    }
    log += sprintf("|after:%y", l[0] instanceof T);
}

# a method of the object removes its last strong reference
sub call() {
    put("a");
    list<auto> l = ();
    l[0] := g_h.a;
    foreach T w in (\l) {
        w.clear();
        log += "|s";
    }
    log += sprintf("|after:%y", l[0] instanceof T);
}

# an untyped variable
sub untyped() {
    put("a");
    list<auto> l = ();
    l[0] := g_h.a;
    foreach auto w in (\l) {
        remove g_h.a;
        log += "|s";
    }
    log += sprintf("|after:%y", l[0] instanceof T);
}

# the object outlives the loop: the element stays a weak reference to it
sub kept() {
    put("a");
    list<auto> l = ();
    l[0] := g_h.a;
    foreach T w in (\l) {
        log += "|s" + w.n;
    }
    remove g_h.a;
    log += sprintf("|after:%y", l[0] instanceof T);
}

# the variable is assigned another object, which the element holds afterwards
sub replaced() {
    put("a");
    list<auto> l = ();
    l[0] := g_h.a;
    foreach T w in (\l) {
        w = new T("b");
        remove g_h.a;
        log += "|s";
    }
    log += sprintf("|after:%y", l[0].n);
}

# several elements, weak and strong, and a break: the elements after it are kept as they are
sub several() {
    put("a");
    put("b");
    put("c");
    list<auto> l = ();
    l[0] := g_h.a;
    l[1] = g_h.b;
    l[2] := g_h.c;
    foreach T w in (\l) {
        log += "|s" + w.n;
        if (w.n == "b") {
            break;
        }
    }
    remove g_h.a;
    remove g_h.b;
    remove g_h.c;
    log += sprintf("|after:%y,%y,%y", l[0] instanceof T, l[1] instanceof T, l[2] instanceof T);
}

noCall();
report("noCall");
call();
report("call");
untyped();
report("untyped");
kept();
report("kept");
replaced();
report("replaced");
several();
report("several");
printf("end: %s\n", log);
