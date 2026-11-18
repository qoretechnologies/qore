#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# methods that a class declares with the name of a parent class's method, in a class with more than one subclass and
# in diamond hierarchies: the inheritance list of each method holds each ancestor method once, so a call tests each
# variant once (qore#6466, qore#6488)

%modern

class A {
    static int id(int v) { return v; }
    int m(int v) { return v; }
}
class B inherits A {
    static string id(string s) { return "B " + s; }
    string m(string s) { return "B " + s; }
}
# more than one subclass, which resolve the hierarchy of B again
class C inherits B {}
class D inherits B {
    static float id(float f) { return f * 2; }
}
class E inherits D {}

# a generic parent class
class G<T> {
    static T gid(T v) { return v; }
}
class HX inherits G<float> {
    static string gid(string s) { return "HX " + s; }
}
class HXX inherits HX {}
class HXY inherits HX {}

# a diamond: A is reached through B and through C (issue #6488)
class DA {
    static int sid(int v) { return v; }
    int m(int v) { return v; }
}
class DB inherits DA {
    static string sid(string s) { return "DB " + s; }
    string m(string s) { return "DB " + s; }
}
class DC inherits DA {
    static bool sid(bool b) { return !b; }
    bool m(bool b) { return !b; }
}
class DD inherits DB, DC {
    static string sid(date d) { return format_date("YYYY-MM-DD", d); }
    string m(date d) { return format_date("YYYY-MM-DD", d); }
}
# a second diamond level over the first: DD is reached through DE and through DF, DA through four paths
class DE inherits DD {}
class DF inherits DD {
    static int sid(binary b) { return b.size(); }
    int m(binary b) { return b.size(); }
}
class DG inherits DE, DF {
    static int sid(hash<auto> h) { return h.size(); }
    int m(hash<auto> h) { return h.size(); }
}
# and a third
class DH inherits DG {}
class DI inherits DG {}
class DJ inherits DH, DI {
    static string sid(code c) { return c(); }
    string m(code c) { return c(); }
}

# a diamond whose second path to the shared ancestor is private: the ancestor is listed once, with the access of its
# first path, so the private path no longer hides the parent class after it (PX)
class PA {
    string m(int i) { return "PA"; }
    static string s(int i) { return "PA"; }
}
class PB inherits PA {}
class PC inherits private PA {}
class PX {
    string m(string str) { return "PX"; }
    static string s(string str) { return "PX"; }
}
class PD inherits PB, PC, PX {
    string m(date d) { return "PD"; }
    static string s(date d) { return "PD"; }
}

#! returns the variants listed by the RUNTIME-OVERLOAD-ERROR of a call, in their order, with how often each is listed
hash<string, int> sub tested(code c) {
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        hash<string, int> rv = {};
        foreach string line in (ex.desc.split("\n")) {
            if (line =~ /^ +::/) {
                string v = trim(line);
                rv{v} = (rv{v} ?? 0) + 1;
            }
        }
        return rv;
    }
    return {"no exception": 1};
}

list<string> sub run() {
    list<string> out = ();
    auto v = (1,);
    # each variant is listed once
    push out, sprintf("B::id: %y", tested(sub () { B::id(v); }));
    push out, sprintf("C::id: %y", tested(sub () { C::id(v); }));
    push out, sprintf("E::id: %y", tested(sub () { E::id(v); }));
    B b();
    push out, sprintf("B.m: %y", tested(sub () { b.m(v); }));
    C c();
    push out, sprintf("C.m: %y", tested(sub () { c.m(v); }));
    push out, sprintf("HX::gid: %y", tested(sub () { HX::gid(v); }));
    # diamonds: each ancestor is listed once
    push out, sprintf("DD::sid: %y", tested(sub () { DD::sid(v); }));
    DD dd();
    push out, sprintf("DD.m: %y", tested(sub () { dd.m(v); }));
    push out, sprintf("DG::sid: %y", tested(sub () { DG::sid(v); }));
    DG dg();
    push out, sprintf("DG.m: %y", tested(sub () { dg.m(v); }));
    push out, sprintf("DJ::sid: %y", tested(sub () { DJ::sid(v); }));
    DJ dj();
    push out, sprintf("DJ.m: %y", tested(sub () { dj.m(v); }));
    # overload resolution through the diamonds
    push out, sprintf("diamond calls: %y", (DD::sid(1), DD::sid("x"), DD::sid(True), DD::sid(2026-01-01),
        dd.m(2), dd.m("y"), dd.m(False), dd.m(2026-01-02), DG::sid(3), DG::sid("z"), DG::sid(<0102>), DG::sid({"a": 1}),
        dg.m(4), dg.m(<010203>), dg.m({}), DJ::sid(5), DJ::sid("v"), DJ::sid(string sub () { return "c"; }),
        DJ::sid(<01>), dj.m(6), dj.m(True), dj.m(string sub () { return "d"; })));
    PD pd();
    push out, sprintf("access paths: %y", (pd.m(1), pd.m("s"), pd.m(2026-01-01), PD::s(1), PD::s("s"),
        PD::s(2026-01-01)));
    # overload resolution through the subclasses
    push out, sprintf("calls: %y", (B::id(1), B::id("x"), C::id(2), C::id("y"), E::id(3), E::id(1.5), E::id("z"),
        b.m(4), b.m("w"), c.m(5), HX::gid("v")));
    return out;
}

# runs more than once, so that the tiered execution mode compiles the code
list<string> out;
for (int i = 0; i < 3; ++i) {
    out = run();
}
map printf("%s\n", $1), out;
