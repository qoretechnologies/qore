# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A member of a class inherited with private:internal and a member of the same name of another base class each keep
# their own declaration, type and value (#6670); see private-internal-member-type.qtest

%modern

class X {
    string m() { return "X"; }
}
class Y {
    string m() { return "Y"; }
}

class G {
    public {
        int v = 1;
        X o();
    }

    setv(auto x) { v = x; }
    seto(auto x) { o = x; }
    incv() { ++v; }
    string get() { auto x = v; return sprintf("%y (%s) %s", v, x.type(), o.m()); }
}

class P inherits private:internal G {
    psetv(auto x) { setv(x); }
    pseto(auto x) { seto(x); }
    pincv() { incv(); }
    string pget() { return get(); }
}

class Q {
    public {
        string v = "q";
        Y o();
    }

    setv(auto x) { v = x; }
    seto(auto x) { o = x; }
    string get() { auto x = v; return sprintf("%y (%s) %s", v, x.type(), o.m()); }
}

class C1 inherits P, Q {}
class C2 inherits Q, P {}

sub t(string label, code c) {
    try {
        c();
        printf("  %s: ok\n", label);
    } catch (hash<ExceptionInfo> ex) {
        printf("  %s: %s: %s\n", label, ex.err, ex.desc);
    }
}

sub check(object o) {
    printf("%s:\n", o.className());
    t("G: v = \"s\"", sub () { o.psetv("s"); });
    t("G: v = 5", sub () { o.psetv(5); });
    t("G: ++v", sub () { o.pincv(); });
    t("G: o = new Y()", sub () { o.pseto(new Y()); });
    t("G: o = new X()", sub () { o.pseto(new X()); });
    t("Q: v = 5", sub () { o.setv(5); });
    t("Q: v = \"x\"", sub () { o.setv("x"); });
    t("Q: o = new X()", sub () { o.seto(new X()); });
    printf("  G: %s\n", o.pget());
    printf("  Q: %s\n", o.get());
    printf("  o.v: %y\n", o.v);
}

check(new C1());
check(new C2());
