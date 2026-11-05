#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

%modern
%allow-debugger

# Each case changes a variable that holds an object with an update that releases the object: the object's destructor
# reads the variable, and the variable that refers to it through a reference, and reads the local variables of the
# updating frame, which reads the variable through the reference.  The update holds the variable's lock while it
# changes the value, so the destructor must run once the lock has been released: run under it, the destructor waits
# for a lock that its own thread holds (an assertion in a DEBUG build).  See lvalue-release-unlocked.qtest.

#! the names of the destructors run, in order
list<string> dtors = ();

class Reader {
    public {
        string name;
    }

    constructor(string name) {
        self.name = name;
    }

    destructor() {
        # read every variable that an update can lock
        int n = (gh ? 1 : 0) + (gl ? 1 : 0) + (go ? 1 : 0) + (Holder::sh ? 1 : 0) + (Holder::sl ? 1 : 0)
            + (Holder::so ? 1 : 0);
        # and the locals of the updating frame, among them a reference to the variable updated
        *hash<auto> vars = get_local_vars(1);
        dtors += sprintf("%s:%d:%d", name, n, vars.r ? 1 : 0);
    }
}

#! an object with a member that holds a Reader
class MemberHolder {
    public {
        auto x;
        auto y = 1;
    }

    constructor(string name) {
        x = new Reader(name);
    }
}

class Holder {
    public {
        static auto sh;
        static auto sl;
        static auto so;
    }
}

our auto gh;
our auto gl;
our auto go;

#! a hash with a Reader in key "a"
hash<auto> sub readerHash(string name) {
    hash<auto!> h = {"b": 1};
    h.a = new Reader(name);
    return h;
}

#! a list with a Reader as its first element
list<auto> sub readerList(string name) {
    list<auto!> l = (1, 1);
    l[0] = new Reader(name);
    return l;
}

# updates through a reference parameter, with the frame's locals read by the destructor
sub refMinus(reference<auto> r) { r -= "a"; }
sub refMinusList(reference<auto> r) { r -= ("a", "c"); }
sub refPlus(reference<auto> r) { r += {"a": 1}; }
sub refExtract(reference<auto> r) { extract r, 0, 1; }
sub refExtractReplace(reference<auto> r) { extract r, 0, 1, 2; }
sub refDelete(reference<auto> r) { delete r; }
sub refMemberMinus(reference<auto> r) { r -= "x"; }
sub refMemberPlus(reference<auto> r) { r += {"x": 1}; }

sub run() {
    gh = readerHash("global-minus"); gh -= "a";
    gh = readerHash("global-minus-list"); gh -= ("a", "c");
    gh = readerHash("global-plus"); gh += {"a": 1};
    gl = readerList("global-extract"); extract gl, 0, 1;
    gl = readerList("global-extract-replace"); extract gl, 0, 1, 2;
    go = new MemberHolder("global-object-minus"); go -= "x";
    go = new MemberHolder("global-object-plus"); go += {"x": 1};
    go = new Reader("global-delete"); delete go;
    Holder::sh = readerHash("static-minus"); Holder::sh -= "a";
    Holder::sh = readerHash("static-plus"); Holder::sh += {"a": 1};
    Holder::sl = readerList("static-extract"); extract Holder::sl, 0, 1;
    Holder::so = new Reader("static-delete"); delete Holder::so;
    gh = readerHash("ref-minus"); refMinus(\gh);
    gh = readerHash("ref-minus-list"); refMinusList(\gh);
    gh = readerHash("ref-plus"); refPlus(\gh);
    gl = readerList("ref-extract"); refExtract(\gl);
    gl = readerList("ref-extract-replace"); refExtractReplace(\gl);
    go = new Reader("ref-delete"); refDelete(\go);
    go = new MemberHolder("ref-object-minus"); refMemberMinus(\go);
    go = new MemberHolder("ref-object-plus"); refMemberPlus(\go);
}

run();
printf("%s\n", dtors.join("\n"));
