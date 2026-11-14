#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# a probe of UnassignedTypedLvalues.qtest (qore#5937): members without a value passed to typed and overloaded
# functions; its output in every execution mode and as a qcc executable must be that of the AST interpreter

%modern
string sub f(int x) { return "int"; }
string sub f(string x) { return "string"; }
string sub f2(int x) { return sprintf("int %y", x); }
string sub f3(*int x) { return sprintf("*int %y", x); }
string sub f3(string x) { return "string"; }
class C {
    public { int m; string s; }
    string a() { return f(m); }
    string b() { return f2(m); }
    string c() { return f3(m); }
    string d() { return f(s); }
}
our int g;
sub t(string l, code c) {
    try {
        printf("%s => %y\n", l, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s => %s: %s\n", l, ex.err, ex.desc);
    }
}
C o();
t("overloaded f(member int NOTHING)", \o.a());
t("f2(int) member NOTHING", \o.b());
t("f3(*int|string) member NOTHING", \o.c());
t("overloaded f(member string NOTHING)", \o.d());
t("f2(int) global NOTHING", auto sub () { return f2(g); });
t("overloaded f(global NOTHING)", auto sub () { return f(g); });
t("f2(int) o.m NOTHING", auto sub () { return f2(o.m); });
