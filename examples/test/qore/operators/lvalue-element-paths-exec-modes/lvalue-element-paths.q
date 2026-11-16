# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the program run by lvalue-element-paths-exec-modes.qtest: one line per case and pass

%modern
%disable-all-warnings

class C {
    public {
        static int x = 0;
        static hash<auto> sh = {"k": 1};
        hash<auto> h = {"k": 1, "a": {"b": 1}};
        int m = 0;
    }
}

hash<auto> gh = {"k": 1, "a": {"b": 1}};

sub show(string label, auto v) { printf("%s => %y %s\n", label, v, v.fullType()); }
sub inc(reference<auto> r) { ++r; }
sub add(reference<auto> r, auto v) { r += v; }
auto sub take(reference<auto> r) { return remove r; }
auto sub id(auto v) { return v; }

sub run(string label, code c) {
    try {
        show(label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s => EXC %s: %s\n", label, ex.err, ex.desc);
    }
}

sub cases(int pass) {
    string k = "k";
    string p = sprintf("%d|", pass);
    # hash element operators on a local hash
    run(p + "local ++h.k", auto sub () { hash<auto> h = {"k": 1}; ++h.k; h.k++; return h; });
    run(p + "local h.k += 1", auto sub () { hash<auto> h = {"k": 1}; h.k += 1; h.k -= 3; h.k *= 2; return h; });
    run(p + "local h{k} += 1", auto sub () { hash<auto> h = {"k": 1}; h{k} += 1; h{k + ""} += 2; return h; });
    run(p + "local h.k = h.k + 1", auto sub () { hash<auto> h = {"k": 1}; h.k = h.k + 1; return h; });
    run(p + "local h.a.b += 1", auto sub () { hash<auto> h = {"a": {"b": 1}}; h.a.b += 1; ++h.a{"b"}; return h; });
    run(p + "local new key", auto sub () { hash<auto> h = {}; ++h.n; h.a.b += 2; h{k} += "s"; return h; });
    run(p + "typed ++h.k", auto sub () { hash<string, int> h = {"k": 0}; ++h.k; h.k += 2; h{k} += 3; return h; });
    run(p + "typed float", auto sub () { hash<string, float> h = {"k": 2.5}; ++h.k; h.k += 2; return h; });
    run(p + "list elem", auto sub () { list<auto> l = ({"k": 0},); l[0].k += 1; ++l[0]{k}; return l; });
    run(p + "object hash", auto sub () { C o(); o.h.k += 1; ++o.h.a.b; ++o.m; o.m += 2; return (o.h, o.m); });
    run(p + "remove/delete", auto sub () { hash<auto> h = {"k": 1, "j": 2, "a": {"b": 3}}; auto r = remove h.k;
        delete h.j; auto r2 = remove h.a.b; return (h, r, r2); });
    run(p + "wrong type", auto sub () { hash<string, int> h = {"k": 0}; h.k += id("x"); return h; });
    run(p + "not a hash", auto sub () { auto h = 1; h.k += 1; return h; });
    # global hash
    run(p + "global", auto sub () { ++gh.k; gh.k += 2; gh.a.b += 1; gh{k} -= 1; return gh; });
    # references to hash elements
    run(p + "ref \\h.k", auto sub () { hash<auto> h = {"k": 1}; inc(\h.k); add(\h.k, 2); return h; });
    run(p + "ref \\h{k}", auto sub () { hash<auto> h = {"k": 1}; inc(\h{k}); add(\h{k + ""}, 2); return h; });
    run(p + "ref \\h.a.b", auto sub () { hash<auto> h = {"a": {"b": 1}}; inc(\h.a.b); add(\h.a{"b"}, 2); return h; });
    run(p + "ref \\h.a{k}", auto sub () { hash<auto> h = {"a": {"k": 1}}; inc(\h.a{k}); add(\h{"a"}{k}, 2); return h; });
    run(p + "ref \\l[0].k", auto sub () { list<auto> l = ({"k": 1},); inc(\l[0].k); add(\l[0]{k}, 2); return l; });
    run(p + "ref \\o.h.k", auto sub () { C o(); inc(\o.h.k); add(\o.h.a.b, 2); inc(\o.m); return (o.h, o.m); });
    run(p + "ref \\gh.k", auto sub () { inc(\gh.k); add(\gh.a.b, 2); return gh; });
    run(p + "ref remove", auto sub () { hash<auto> h = {"k": 1, "j": 2}; auto r = take(\h.k); return (h, r); });
    run(p + "ref typed", auto sub () { hash<string, int> h = {"k": 1}; inc(\h.k); add(\h{k}, 2); return h; });
    run(p + "ref wrong type", auto sub () { hash<string, int> h = {"k": 1}; add(\h.k, "x"); return h; });
    # static class variables
    run(p + "static ++", auto sub () { ++C::x; C::x++; C::x += 3; C::x -= 1; C::x = C::x * 2; return C::x; });
    run(p + "static hash", auto sub () { ++C::sh.k; C::sh.k += 2; C::sh{k} += 1; return C::sh; });
    run(p + "static ref", auto sub () { inc(\C::x); add(\C::sh.k, 2); return (C::x, C::sh); });
    run(p + "static remove", auto sub () { int v = C::x; auto r = remove C::sh.k; C::sh.k = v; return (r, C::sh); });
}

for (int i = 0; i < 3; ++i) {
    cases(i);
}
