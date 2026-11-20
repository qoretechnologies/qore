# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A member of C0 reached through a private:internal path and a public path by a class declared in a child Program
# that imports the classes of the paths: one value, seen through every path that can access it, whatever the order
# of the parent classes (the imported-class form of qore#6668); see imported-private-internal-paths.qtest

%modern
%no-child-restrictions

class C0 {
    public { int m0 = 10; }
    constructor() { m0 = 11; }
    auto get0() { return m0; }
    set0(int v) { m0 = v; }
}
class PI inherits private:internal C0 { auto getpi() { return m0; } }
class PU inherits C0 { auto getpu() { return m0; } }
class PI2 inherits PI {}
class PU2 inherits PU {}

const Decls = {
    "A1": "class A1 inherits PI, PU { auto geta() { return m0; } }",
    "A2": "class A2 inherits PU, PI { auto geta() { return m0; } }",
    "A3": "class A3 inherits PI2, PU2 { auto geta() { return m0; } }",
    "A4": "class A4 inherits PU2, PI2 { auto geta() { return m0; } }",
    "A5": "class A5 inherits PI, C0 { auto geta() { return m0; } }",
    "A6": "class A6 inherits C0, PI { auto geta() { return m0; } }",
    "A7": "class A7 inherits private:internal PU, PU2 { auto geta() { return m0; } }",
    "A8": "class A8 inherits PU2, private:internal PU { auto geta() { return m0; } }",
    "A9": "class A9 inherits PI {}",
};

# the child Programs, which must live as long as their objects
list<Program> Children = ();

# a child Program per class, which imports the parent classes
object sub mk(string n) {
    Program p(PO_NEW_STYLE | PO_NO_INHERIT_USER_CLASSES);
    map p.importClass($1), ("C0", "PI", "PU", "PI2", "PU2");
    Children += p;
    p.parse(Decls{n} + "\nobject sub mk() { return new " + n + "(); }\n", "child");
    return p.callFunction("mk");
}

sub t(string n) {
    object o;
    try { o = mk(n); } catch (hash<ExceptionInfo> ex) { printf("%s: %s: %s\n", n, ex.err, ex.desc); return; }
    list<auto> l = ();
    foreach string m in (("get0", "getpi", "getpu", "geta")) {
        try { l += call_object_method(o, m); } catch (hash<ExceptionInfo> ex) { l += "-"; }
    }
    auto outside;
    try { outside = o.m0; } catch (hash<ExceptionInfo> ex) { outside = ex.err; }
    printf("%s: outside=%y methods=%y\n", n, outside, l);
    try { o.set0(7); } catch (hash<ExceptionInfo> ex) {}
    l = ();
    foreach string m in (("get0", "getpi", "getpu", "geta")) {
        try { l += call_object_method(o, m); } catch (hash<ExceptionInfo> ex) { l += "-"; }
    }
    try { outside = o.m0; } catch (hash<ExceptionInfo> ex) { outside = ex.err; }
    printf("%s after set0(7): outside=%y methods=%y\n", n, outside, l);
}
map t($1), ("A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9");
