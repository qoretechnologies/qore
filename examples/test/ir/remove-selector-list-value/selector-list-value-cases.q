#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the cases of remove-selector-list-value.qtest: each prints its result, or the exception it raised; the output of
# every execution mode must be that of the AST interpreter

%modern

# a list given as the value of a selector: untyped, so that the parser cannot refuse it as an index
auto ix = (1, 2);
our auto gl;
our auto gn;

class D {
    public { string name; }
    constructor(string n) { name = n; }
    destructor() { printf("  destroy %s\n", name); }
}

class M {
    public { auto a = 1; auto b = 2; auto c = 3; }
}

class L {
    public { list<auto> l = (10, 11, 12, 13); }
}

int sub thrower() {
    throw "SEL-ERROR";
}

sub t(string name, code<auto()> c) {
    printf("%s:\n", name);
    try {
        printf("  = %y\n", c());
    } catch (hash<ExceptionInfo> ex) {
        printf("  ! %s: %s\n", ex.err, ex.desc);
    }
}

sub main() {
    # qore#5736: in a list of selectors, a selector whose value is a list is one index (getAsBigInt(): 0), as in
    # the value and the iteration of the same list of selectors; only a range selects each index of its value
    t("value list selector", auto sub () { list<auto> l = (10, 11, 12, 13); return l[0, ix, 3..3]; });
    t("iterate list selector", auto sub () {
        list<auto> l = (10, 11, 12, 13);
        list<auto> r = ();
        foreach auto v in (l[0, ix, 3..3]) {
            r += v;
        }
        return r;
    });
    t("remove list selector", auto sub () {
        list<auto> l = (10, 11, 12, 13);
        auto r = remove l[0, ix, 3..3];
        return (r, l);
    });
    t("delete list selector", auto sub () {
        list<auto> l = (10, 11, 12, 13);
        delete l[0, ix, 3..3];
        return l;
    });
    t("remove list selector first", auto sub () {
        list<auto> l = (10, 11, 12, 13);
        auto r = remove l[ix, 2];
        return (r, l);
    });
    t("remove string list selector", auto sub () {
        string s = "abcd";
        auto r = remove s[0, ix, 3..3];
        return (r, s);
    });
    t("remove binary list selector", auto sub () {
        binary b = <00010203>;
        auto r = remove b[0, ix, 3..3];
        return (r, b);
    });
    t("remove global list selector", auto sub () {
        gl = (10, 11, 12, 13);
        auto r = remove gl[0, ix];
        return (r, gl);
    });
    t("remove path list selector", auto sub () {
        hash<auto> h = {"a": (10, 11, 12, 13)};
        auto r = remove h.a[0, ix];
        return (r, h);
    });
    t("remove ternary selector", auto sub () {
        # the ternary is not a range, so its list value is one index
        list<auto> l = (10, 11, 12, 13);
        bool c = True;
        auto r = remove l[0, c ? ix : 1..2];
        return (r, l);
    });
    t("remove ternary range selector", auto sub () {
        list<auto> l = (10, 11, 12, 13);
        bool c = False;
        auto r = remove l[0, c ? ix : 1..2];
        return (r, l);
    });
    t("remove list selector out of range", auto sub () {
        list<auto> l = (10, 11, 12, 13);
        auto big = (7, 8);
        auto r = remove l[3, big];
        return (r, l);
    });
    t("delete object list selector", auto sub () {
        list<auto> l = (new D("d0"), new D("d1"), new D("d2"));
        delete l[2, ix];
        return (map $1.name, l);
    });
    t("partial remove list selector", auto sub () {
        # the selectors before the one that raises remove what they select
        list<auto> l = (10, 11, 12, 13);
        try {
            remove l[ix, 3..3, thrower()];
        } catch (hash<ExceptionInfo> ex) {
            printf("  caught %s\n", ex.err);
        }
        return l;
    });
    t("partial remove string list selector", auto sub () {
        string s = "abcd";
        try {
            remove s[ix, 3..3, thrower()];
        } catch (hash<ExceptionInfo> ex) {
            printf("  caught %s\n", ex.err);
        }
        return s;
    });
    t("delete string list selector", auto sub () { string s = "abcd"; delete s[0, ix, 3..3]; return s; });
    t("delete binary list selector", auto sub () { binary b = <00010203>; delete b[0, ix, 3..3]; return b; });
    t("remove reference list selector", auto sub () {
        list<auto> l = (10, 11, 12, 13);
        reference<auto> r = \l;
        auto x = remove r[0, ix];
        return (x, l);
    });
    t("remove member list selector", auto sub () {
        L o();
        auto x = remove o.l[0, ix];
        return (x, o.l);
    });
    t("remove nested list selector", auto sub () {
        list<auto> l = (1, (10, 11, 12));
        auto r = remove l[1][0, ix];
        return (r, l);
    });
    t("remove nested range", auto sub () {
        list<auto> l = (1, (10, 11, 12));
        auto r = remove l[1][0..1];
        return (r, l);
    });
    # in a list of hash keys, a key whose value is a list is one key (its string value); a single key whose value is
    # a list is a slice
    t("remove hash list key", auto sub () {
        hash<auto> h = {"a": 1, "b": 2, "c": 3, "b c": 4};
        auto kl = ("b", "c");
        auto r = remove h{"a", kl};
        return (r, h);
    });
    t("delete hash list key", auto sub () {
        hash<auto> h = {"a": 1, "b": 2, "c": 3};
        auto kl = ("b", "c");
        delete h{"a", kl};
        return h;
    });
    t("remove object list key", auto sub () {
        M m();
        auto kl = ("b", "c");
        auto r = remove m{"a", kl};
        return (r, m.a, m.b, m.c);
    });
    t("remove hash single list key", auto sub () {
        hash<auto> h = {"a": 1, "b": 2, "c": 3};
        auto kl = ("b", "c");
        auto r = remove h.(kl);
        return (r, h);
    });
    # a single index whose value is a list on a value that is not a list, a string or a binary removes the value
    t("remove single list index int", auto sub () { auto n = 5; auto r = remove n[ix]; return (r, n); });
    t("remove single list index hash", auto sub () { auto n = {"a": 1}; auto r = remove n[ix]; return (r, n); });
    t("remove single list index path", auto sub () { auto h = {"a": 5}; auto r = remove h.a[ix]; return (r, h); });
    t("delete single list index int", auto sub () { auto n = 5; delete n[ix]; return n; });
    # ranges and constants: unchanged
    t("remove range selectors", auto sub () {
        list<auto> l = (10, 11, 12, 13);
        auto r = remove l[0, 1..2];
        return (r, l);
    });
    t("remove reverse range selector", auto sub () {
        list<auto> l = (10, 11, 12, 13);
        auto r = remove l[3..2, 0];
        return (r, l);
    });
    t("remove constant selectors", auto sub () {
        list<auto> l = (10, 11, 12, 13);
        auto r = remove l[0, 2];
        return (r, l);
    });
    # a single index whose value is a list is a slice in every mode
    t("remove single list index", auto sub () {
        list<auto> l = (10, 11, 12, 13);
        auto r = remove l[ix];
        return (r, l);
    });

    # qore#5777: a single range on a value that is not a list, a string, a binary or a buffer raises
    # ILLEGAL-EXPRESSION, as the value form does
    t("value range int", auto sub () { auto n = 5; return n[0..1]; });
    t("remove range nothing", auto sub () { auto n; return remove n[0..1]; });
    t("delete range nothing", auto sub () { auto n; delete n[0..1]; return n; });
    t("remove range int", auto sub () { auto n = 5; auto r = remove n[0..1]; return (r, n); });
    t("delete range int", auto sub () { auto n = 5; delete n[0..1]; return n; });
    t("remove range hash", auto sub () { auto n = {"a": 1}; auto r = remove n[0..1]; return (r, n); });
    t("remove range object", auto sub () {
        auto n = new D("ro");
        auto r = remove n[0..1];
        return (r, n.name);
    });
    t("remove range path int", auto sub () { auto h = {"a": 5}; auto r = remove h.a[0..1]; return (r, h); });
    t("remove range global nothing", auto sub () { remove gn; return remove gn[0..1]; });
    t("remove range through reference", auto sub () {
        auto n = 5;
        reference<auto> rn = \n;
        auto r = remove rn[0..1];
        return (r, n);
    });
    # a missing path is not created and nothing is removed
    t("remove range missing path", auto sub () { hash<auto> h = {}; auto r = remove h.a[0..1]; return (r, h); });
    # a buffer: nothing is removed
    t("remove range buffer", auto sub () {
        auto b = new buffer<int64>((1, 2, 3));
        auto r = remove b[0..1];
        return (r, b.size());
    });
    # a list, a string and a binary: unchanged
    t("remove range list", auto sub () { list<auto> l = (10, 11, 12); auto r = remove l[0..1]; return (r, l); });
    t("remove range string", auto sub () { string s = "abc"; auto r = remove s[1..2]; return (r, s); });
    t("remove range binary", auto sub () { binary b = <000102>; auto r = remove b[2..1]; return (r, b); });
    # a list of selectors on a value that is not a container removes nothing (qore#5695)
    t("remove selectors int", auto sub () { auto n = 5; auto r = remove n[0, 1..2]; return (r, n); });
}

main();
