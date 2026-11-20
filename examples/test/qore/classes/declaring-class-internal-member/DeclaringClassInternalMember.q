# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The class that declares a member, and the classes between it and the class that inherits it with private:internal,
# use the member on an object of a subclass through any reference, as through self (#6800), and the other classes
# still cannot (#6760); see declaring-class-internal-member.qtest

%modern
%no-child-restrictions

class G {
    public {
        int v = 1;
    }

    auto gread(object o) { return o.v; }
    auto gdyn(object o) { return o.("v"); }
    auto gwrite(object o) { o.v = 2; return o.v; }
    auto gself() { return v; }
    auto gkeys(object o) { return keys o; }
    auto gkself() { return keys self; }
}

class P inherits private:internal G {
    auto pread(object o) { return o.v; }
    auto pgread(object o) { return gread(o); }
    auto pgdyn(object o) { return gdyn(o); }
    auto pgwrite(object o) { return gwrite(o); }
    auto pgself() { return gself(); }
    auto pgkeys(object o) { return gkeys(o); }
    auto pgkself() { return gkself(); }
}

# the subclass of the inheriting class cannot use the member
class C inherits P {
    auto cread(object o) { return o.v; }
    auto cdyn(object o) { return o.("v"); }
    auto ckeys(object o) { return keys o; }
}

# a class with a private member beside the others
class A {
    private {
        int pm = 5;
    }
}

# a base class beside the inheriting class
class S {
    auto sread(object o) { return o.v; }
    auto sdyn(object o) { return o.("v"); }
    auto skeys(object o) { return keys o; }
    auto spm(object o) { return o.pm; }
    auto spmdyn(object o) { return o.("pm"); }
}

class Q inherits A, P, S {}

class Other {
    auto oread(object o) { return o.v; }
    auto odyn(object o) { return o.("v"); }
    auto okeys(object o) { return keys o; }
    auto opm(object o) { return o.pm; }
}

sub t(string label, code c) {
    try {
        printf("%s: %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

C c();
P p();
Q q();
t("declaring class through self", auto sub () { return c.pgself(); });
t("declaring class through a reference", auto sub () { return c.pgread(c); });
t("declaring class dynamically", auto sub () { return c.pgdyn(c); });
t("declaring class writes through a reference", auto sub () { return c.pgwrite(c); });
t("declaring class: keys through self", auto sub () { return c.pgkself(); });
t("declaring class: keys through a reference", auto sub () { return c.pgkeys(c); });
t("inheriting class through a reference", auto sub () { return c.pread(c); });
t("declaring class on the inheriting class", auto sub () { return p.pgread(p); });
t("declaring class beside a sibling", auto sub () { return q.pgread(q); });
t("subclass of the inheriting class", auto sub () { return c.cread(c); });
t("subclass of the inheriting class dynamically", auto sub () { return c.cdyn(c); });
t("subclass of the inheriting class: keys", auto sub () { return c.ckeys(c); });
t("sibling base class", auto sub () { return q.sread(q); });
t("sibling base class dynamically", auto sub () { return q.sdyn(q); });
t("sibling base class: keys", auto sub () { return q.skeys(q); });
t("sibling base class: private member", auto sub () { return q.spm(q); });
t("sibling base class: private member dynamically", auto sub () { return q.spmdyn(q); });
t("other class", auto sub () { return (new Other()).oread(c); });
t("other class dynamically", auto sub () { return (new Other()).odyn(c); });
t("other class: keys", auto sub () { return (new Other()).okeys(q); });
t("other class: private member", auto sub () { return (new Other()).opm(q); });
t("outside", auto sub () { auto o = c; return o.v; });

# parses code in a Program of its own and prints the result: the first exception, or the value of get()
sub try_parse(string label, string code) {
    Program pgm(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
    try {
        pgm.parse(code, label);
        printf("%s: parsed: %y\n", label, pgm.callFunction("get"));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

# the classes of the typed cases, with the methods of the case in G and P
string sub classes(string gm = "", string pm = "") {
    return sprintf("class G { public { int v = 1; } %s }\n"
        "class P inherits private:internal G { %s }\n"
        "class C inherits P {}\n"
        "class A { private { int pm = 5; } }\n", gm, pm);
}

# the parse-time answers agree with the run-time ones above
try_parse("typed: declaring class", classes("int gv(C c) { return c.v; }", "int pgv(C c) { return gv(c); }")
    + "auto sub get() { C c(); return c.pgv(c); }\n");
try_parse("typed: declaring class writes", classes("int gw(C c) { c.v = 2; return c.v; }",
    "int pgw(C c) { return gw(c); }") + "auto sub get() { C c(); return c.pgw(c); }\n");
try_parse("typed: inheriting class", classes("", "int pv(C c) { return c.v; }")
    + "auto sub get() { C c(); return c.pv(c); }\n");
try_parse("typed: subclass of the inheriting class", classes()
    + "class D inherits P { int dv(D d) { return d.v; } }\n"
    "auto sub get() { D d(); return d.dv(d); }\n");
try_parse("typed: sibling base class", classes()
    + "class S { int sv(Q q) { return q.v; } }\n"
    "class Q inherits P, S {}\n"
    "auto sub get() { Q q(); return q.sv(q); }\n");
try_parse("typed: sibling base class: private member", classes()
    + "class S { int spm(Q q) { return q.pm; } }\n"
    "class Q inherits A, S {}\n"
    "auto sub get() { Q q(); return q.spm(q); }\n");
try_parse("typed: other class", classes()
    + "class O { int ov(C c) { return c.v; } }\n"
    "auto sub get() { C c(); return (new O()).ov(c); }\n");
