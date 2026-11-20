# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A private member is reachable only from the class that declares it and its subclasses, as a private method is
# (#6760); see sibling-private-member.qtest

%modern
%no-child-restrictions

class A {
    private {
        string pm = "A::pm";
    }

    public {
        string pub = "A::pub";
    }

    # the declaring class uses its private member on an object of a subclass
    string aget(object o) { return o.pm; }
}

class B2 {
    # another base class of the same subclass, which does not inherit A
    auto rd(object o) { return o.pm; }
    auto wr(object o) { o.pm = "B2"; return o.pm; }
    auto dyn(object o) { return o.("pm"); }
    auto ks(object o) { return keys o; }
    auto plus(object o) { hash<auto> h = {}; return h + o; }
    auto fk(object o) { return o.firstKey(); }
    auto pub(object o) { return o.pub; }
}

class X5 inherits A, B2 {
    # the class that inherits the declaring class uses the member
    string xget() { return pm; }
}

class X6 inherits X5 {
    # a subclass of it does too
    string x6get(object o) { return o.pm; }
}

sub t(string label, code c) {
    try {
        printf("%s: %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

X5 o();
X6 o6();
t("other base class: read", auto sub () { return o.rd(o); });
t("other base class: write", auto sub () { return o.wr(o); });
t("other base class: dynamic read", auto sub () { return o.dyn(o); });
t("other base class: keys", auto sub () { return o.ks(o); });
t("other base class: hash + object", auto sub () { return o.plus(o); });
t("other base class: first key", auto sub () { return o.fk(o); });
t("other base class: public member", auto sub () { return o.pub(o); });
t("declaring class", auto sub () { return o.aget(o); });
t("inheriting class", auto sub () { return o.xget(); });
t("subclass", auto sub () { return o6.x6get(o6); });
t("declaring class on a subclass", auto sub () { return o6.aget(o6); });

# parses code in a Program of its own and prints the result: the first exception, or the value of get()
sub try_parse(string label, string code) {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
    try {
        p.parse(code, label);
        printf("%s: parsed: %y\n", label, p.callFunction("get"));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

try_parse("typed: other base class", "class A { private { string pm = \"A::pm\"; } }\n"
    "class B2 { auto rd(X5 o) { return o.pm; } }\n"
    "class X5 inherits A, B2 {}\n"
    "auto sub get() { X5 o(); return o.rd(o); }\n");
try_parse("typed: subclass", "class A { private { string pm = \"A::pm\"; } }\n"
    "class B2 {}\n"
    "class X5 inherits A, B2 { auto rd(X5 o) { return o.pm; } }\n"
    "auto sub get() { X5 o(); return o.rd(o); }\n");
