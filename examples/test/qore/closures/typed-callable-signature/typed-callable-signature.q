#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# the signature of a typed callable is checked at run time (qore#6660): each case prints the type of what it stored,
# or the error; the callables are held by "auto" variables, so that only the run-time check sees their signatures

%modern

int sub fi(int x) { return x + 1; }
string sub fs() { return "s"; }
int sub fov(int x) { return x; }
string sub fov(string x) { return x; }

class C {
    public {
        code<int(int)> op;
        *code<int(int)> opt;
    }

    int m(int x) { return x * 2; }
    string s() { return "s"; }
}

# typed callable members of different signatures (qore#6660: values of a literal must not be folded to the
# narrower signature)
hashdecl Callbacks {
    code<nothing(string)> sql;
    code<nothing(string, int, string, *string, *string)> info;
}

int sub take_opts(hash<auto> opts) {
    opts.sql("s");
    opts.info("s", 1, "t");
    return opts.size();
}

int sub take_callbacks(hash<Callbacks> cbs) {
    cbs.sql("s");
    cbs.info("s", 1, "t");
    return 2;
}

sub pf(code<int(int)> c) {}
sub pfn(*code<int(int)> c) {}
code<int(int)> sub rf(auto c) { return c; }

auto sub get_bad() { return string sub () { return "x"; }; }
auto sub get_ok() { return int sub (int x) { return x + 10; }; }

sub t(string label, code c) {
    try {
        printf("%s: %s\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

# rejected
t("var", string sub () { auto c = get_bad(); code<int(int)> d = c; return d.fullType(); });
t("var param type", string sub () { auto c = int sub (string x) { return 1; }; code<int(int)> d = c; return "accepted"; });
t("var param count", string sub () { auto c = int sub (int x, int y) { return 1; }; code<int(int)> d = c;
    return "accepted"; });
t("var reassign", string sub () { code<int(int)> d = get_ok(); d = get_bad(); return "accepted"; });
t("param", string sub () { pf(get_bad()); return "accepted"; });
t("param or nothing", string sub () { pfn(get_bad()); return "accepted"; });
t("return", string sub () { auto r = rf(get_bad()); return "accepted"; });
t("member", string sub () { C o(); o.op = get_bad(); return "accepted"; });
t("member or nothing", string sub () { C o(); o.opt = get_bad(); return "accepted"; });
t("list push", string sub () { list<code<int(int)>> l = (); push l, get_bad(); return l[0].fullType(); });
t("list assign", string sub () { auto c = get_bad(); list<code<int(int)>> l = (c,); return l.fullType(); });
t("list element", string sub () { list<code<int(int)>> l = (get_ok(),); l[0] = get_bad(); return "accepted"; });
t("hash assign", string sub () { auto c = get_bad(); hash<string, code<int(int)>> h = {"a": c};
    return h.fullType(); });
t("hash key", string sub () { hash<string, code<int(int)>> h(); h.a = get_bad(); return "accepted"; });
t("call ref", string sub () { auto c = \fs(); code<int(int)> d = c; return "accepted"; });
t("method ref", string sub () { C o(); auto c = \o.s(); code<int(int)> d = c; return "accepted"; });
t("builtin ref", string sub () { auto c = \strlen(); code<string(int)> d = c; return "accepted"; });

# accepted
t("ok var", string sub () { auto c = get_ok(); code<int(int)> d = c; return sprintf("%s %d", d.fullType(), d(1)); });
t("ok contravariant param", string sub () { auto c = int sub (auto x) { return 1; }; code<int(int)> d = c;
    return string(d(1)); });
t("ok covariant return", string sub () { auto c = int sub (int x) { return x; }; code<auto(int)> d = c;
    return string(d(2)); });
t("ok default arg", string sub () { auto c = int sub (int x, *int y) { return x; }; code<int(int)> d = c;
    return string(d(3)); });
t("ok nothing return", string sub () { auto c = int sub (string s) { return 1; }; code<nothing(string)> d = c;
    return "accepted"; });
t("ok untyped code", string sub () { auto c = get_bad(); code d = c; return d(); });
t("ok param", string sub () { pf(get_ok()); return "accepted"; });
t("ok param nothing", string sub () { pfn(); return "accepted"; });
t("ok return", string sub () { auto r = rf(get_ok()); return string(r(1)); });
t("ok member", string sub () { C o(); o.op = get_ok(); return string(o.op(1)); });
t("ok list", string sub () { auto c = get_ok(); list<code<int(int)>> l = (c,); push l, c;
    return sprintf("%s %d", l.fullType(), l[1](1)); });
t("ok hash", string sub () { hash<string, code<int(int)>> h(); h.a = get_ok();
    return sprintf("%s %d", h.fullType(), h.a(1)); });
t("ok call ref", string sub () { auto c = \fi(); code<int(int)> d = c; return string(d(1)); });
t("ok overloaded call ref", string sub () { auto c = \fov(); code<int(int)> d = c; return string(d(4)); });
t("ok method ref", string sub () { C o(); auto c = \o.m(); code<int(int)> d = c; return string(d(4)); });
t("ok builtin ref", string sub () { auto c = \strlen(); code<int(string)> d = c; return string(d("abc")); });

# literals with callables of different signatures, run often enough for tiered execution to compile the code
t("ok mixed signatures", string sub () {
    int n = 0;
    for (int i = 0; i < 30; ++i) {
        list<string> l = ();
        n += take_opts({
            "sql": sub (string str) { l += str; },
            "info": sub (string str, int ac, string type, *string name, *string info) { l += type; },
        });
        n += take_callbacks(<Callbacks>{
            "sql": sub (string str) { l += str; },
            "info": sub (string str, int ac, string type, *string name, *string info) { l += type; },
        });
        hash<Callbacks> h = <Callbacks>{
            "sql": sub (string str) { l += str; },
            "info": sub (string str, int ac, string type, *string name, *string info) { l += type; },
        };
        n += take_callbacks(h);
        list<code> cl = (sub (string str) {}, sub (string str, int ac) {});
        n += cl.size();
    }
    return string(n);
});
