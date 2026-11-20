# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the value of a return that a handler's exception interrupts in a try block of an outlined helper is released
# before the catch block (qore#6650); see AOTOutlineExceptionScopes.qtest

%modern
our list<string> log = ();
class T { private { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; } }
T sub u2() {
    try {
        T t("u2");
        on_exit { log += "x"; throw "U2"; }
        return t;
    } catch () {
        log += "c";
    }
    return new T("u2-other");
}
{ T r = u2(); }
printf("%y\n", log);
