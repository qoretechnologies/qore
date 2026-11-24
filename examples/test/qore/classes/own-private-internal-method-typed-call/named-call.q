#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# a named call in the code of a class with private:internal variants of a method is bound to the class's own method,
# also on an object of a subclass: one whose names match more than one of its variants is refused as ambiguous, as
# the same call through self is (qore#7440); see ../own-private-internal-method-typed-call.qtest

%modern

const Classes = "class B {
    private:internal int m(string s) { return 1; }
    string m(softstring s) { return \"B::m(softstring)\"; }
    %s
}
class K inherits B {
    string m(softstring s) { return \"K::m(softstring)\"; }
}
";

sub check(string label, string call) {
    Program p(PO_NEW_STYLE);
    try {
        p.parse(sprintf(Classes, call), label);
        printf("%s: no exception\n", label);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

check("subclass object", "auto namedK(K k) { return k.m(s: \"x\"); }");
check("self", "auto namedSelf() { return self.m(s: \"x\"); }");
