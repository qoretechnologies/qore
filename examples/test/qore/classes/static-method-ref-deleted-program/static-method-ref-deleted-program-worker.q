#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the worker of static-method-ref-deleted-program.qtest: static method call references to a method of a class that
# a Program imported, called after the Program is deleted (issue #6622)

%modern
%no-child-restrictions

class C {
    static string m(string s) {
        return "m:" + s;
    }

    #! returns a reference to the static method, made in the code of the class
    static code ref() {
        return \C::m();
    }
}

#! returns the reference that the code parsed in a Program that imports C gives, and deletes the Program
/** the Program also has a class of its own, D, whose method makes a reference (its class context)
*/
code sub make_ref(string code) {
    Program p(PO_MODERN | PO_NO_CHILD_PO_RESTRICTIONS);
    p.importClass("C");
    p.parse("class D { static code ref() { return \\C::m(); } static string m(string s) { return \"d:\" + s; } } "
        "class E inherits C {} "
        "class S { public { static code r = \\S::m(); } static string m(string s) { return \"s:\" + s; } } "
        "code sub get() { " + code + " }", "pgm");
    code rv = p.callFunction("get");
    # the reference can be called while the Program exists
    printf("before: %s\n", rv("x"));
    delete p;
    return rv;
}

#! calls the reference, printing its result or the exception
sub call(string label, code c) {
    try {
        printf("%s: %s\n", label, c("y"));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

map call($1.key, make_ref($1.value)), {
    "written in code": "return \\C::m();",
    "get_call_reference()": "return get_call_reference(\"C::m\");",
    # the code of the imported class runs in the Program that defines it, which is not deleted
    "made in a method of the imported class": "return C::ref();",
    "made in a method of a class of the Program": "return D::ref();",
    "a method of a class of the Program": "return \\D::m();",
    "through a subclass of the imported class": "return \\E::m();",
    "held by a static variable of its class": "return S::r;",
}.pairIterator();

# a reference is called more than once after the Program is deleted
code c = make_ref("return \\C::m();");
call("first call", c);
call("second call", c);
# the classes are released with the reference
delete c;
printf("done\n");
