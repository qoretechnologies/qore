# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the exit blocks of catch blocks in a helper called in a catch block of the coordinator close the catch blocks
# opened since their try statements began, counted from those open when the helper is called (qore#6651); see
# AOTOutlineExceptionScopes.qtest

%modern
our list<string> log = ();
class T { private { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; } }
sub k1() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        log += "a:" + o.err;
        T t1("t1");
        log += "b";
        try {
            T t2("t2");
            throw "I";
        } catch (hash<ExceptionInfo> i) {
            log += i.err;
            try { throw "J"; } catch (hash<ExceptionInfo> j) { log += j.err; throw "K"; }
        }
        log += "never";
    }
}
sub k2() {
    try { throw "O2"; } catch (hash<ExceptionInfo> o) {
        try { k1(); } catch (hash<ExceptionInfo> e) { log += "k1:" + e.err; }
        log += "c";
        try { throw "I2"; } catch (hash<ExceptionInfo> i) { log += i.err; }
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "o:" + r.err; }
    }
}
k2();
printf("%y\n", log);
