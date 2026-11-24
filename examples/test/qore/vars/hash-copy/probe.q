#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the output of the copies of hashes and of their changes and lookups; the same in every execution mode (qore#7265)

%modern
%enable-all-warnings

hashdecl HD {
    int a = 1;
    string b = "two";
    *hash<auto> c;
}

hashdecl HD2 inherits HD {
    list<auto> d = (1, 2);
}

# a class without a member list: objects get members at run time
class Holder {
    constructor() {
        foreach hash<auto> p in ({"x": 1, "h": {"k": 1}}.pairIterator()) {
            self{p.key} = p.value;
        }
    }
}

# every key of the hash is found with its value, a key that it does not have is not, and the keys are in order
string sub verify(hash<auto> h) {
    list<string> errs = ();
    int n = 0;
    foreach hash<auto> p in (h.pairIterator()) {
        if (!h.hasKey(p.key)) {
            push errs, sprintf("no key %y", p.key);
        } else if (h{p.key} != p.value) {
            push errs, sprintf("value of %y", p.key);
        }
        ++n;
    }
    if (n != h.size()) {
        push errs, sprintf("size %d keys %d", h.size(), n);
    }
    foreach string k in ("absent", "k-1", "naïve-absent", "abcdefghijklmnopqrstuvwxyz-absent") {
        if (h.hasKey(k)) {
            push errs, sprintf("has %y", k);
        }
    }
    return errs ? errs.join(", ") : "ok";
}

string sub summary(hash<auto> h) {
    return sprintf("%d %s %s", h.size(), verify(h), (map sprintf("%s=%y", $1.key, $1.value), h.pairIterator())
        .join(","));
}

hash<auto> sub make(int n, string prefix = "k") {
    hash<auto!> h = {};
    for (int i = 0; i < n; ++i) {
        h{prefix + i} = i;
    }
    return h;
}

sub show(string label, code c) {
    try {
        printf("%s: %s\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

sub show(string label, code c, hash<auto> h) {
    show(label, auto sub () { return c(h); });
}

# copies of hashes of many sizes: change the copy, then add and delete keys; the original does not change
foreach int n in (0, 1, 2, 3, 7, 8, 12, 13, 14, 25, 26, 27, 29, 30, 100, 1000) {
    hash<auto!> h = make(n);
    hash<auto!> c = h;
    c.added = n;
    string label = sprintf("size%d", n);
    if (n <= 30) {
        show(label + " copy", \summary(), c);
    } else {
        show(label + " copy", \verify(), c);
        printf("%s copy keys: %d first %y last %y\n", label, c.size(), (keys c)[0], (keys c)[c.size() - 1]);
    }
    for (int i = 0; i < 40; ++i) {
        c{"new" + i} = i;
    }
    for (int i = 0; i < n; i += 2) {
        remove c{"k" + i};
    }
    show(label + " changed copy", \verify(), c);
    printf("%s changed copy keys: %d %y\n", label, c.size(), (keys c)[0..5]);
    show(label + " original", \verify(), h);
    printf("%s original keys: %d\n", label, h.size());
}

# a copy of an empty hash
{
    hash<auto!> h = {};
    hash<auto!> c = h;
    c.a = 1;
    show("empty copy", \summary(), c);
    show("empty original", \summary(), h);
}

# a copy after keys were deleted keeps the order of the remaining keys
{
    hash<auto!> h = make(20);
    foreach int i in (0, 5, 6, 19, 10) {
        delete h{"k" + i};
    }
    h.k5 = "re-added";
    hash<auto!> c = h;
    c.k1 = "changed";
    show("deleted copy", \summary(), c);
    show("deleted original", \summary(), h);
    remove c.k1;
    c.k1 = "last";
    show("deleted copy re-added", \summary(), c);
}

# keys that are not ASCII, empty, long (not in the string's own buffer), and that differ only at the end
{
    hash<auto!> h = {
        "": "empty",
        "naïve": 1,
        "日本語": 2,
        "Ünïcödé-key-longer-than-sixteen-bytes": 3,
        "abcdefghijklmnopqrstuvwxyz0": 4,
        "abcdefghijklmnopqrstuvwxyz1": 5,
        "x": 6,
        "€": 7,
    };
    hash<auto!> c = h;
    c."日本語" = "changed";
    show("non-ascii copy", \summary(), c);
    show("non-ascii original", \summary(), h);
    show("non-ascii lookup", string sub () { return sprintf("%y %y %y", c."naïve", c{""}, c{"€"}); });
    string latin = convert_encoding("naïve", "ISO-8859-1");
    show("non-ascii lookup iso-8859-1", string sub () { return sprintf("%y", c{latin}); });
    show("non-ascii lookup utf-16le", string sub () { return sprintf("%y", c{convert_encoding("€", "UTF-16LE")}); });
}

# nested hashes are copied when they are changed through the copy
{
    hash<auto!> h = {"a": 1, "n": {"x": 1, "y": {"z": 2}}, "l": (1, 2, 3)};
    hash<auto!> c = h;
    c.n.y.z = 3;
    c.n.w = 4;
    push c.l, 4;
    show("nested copy", string sub () { return sprintf("%y", c); });
    show("nested original", string sub () { return sprintf("%y", h); });
    show("nested verify", string sub () { return sprintf("%s %s %s", verify(c), verify(c.n), verify(c.n.y)); });
}

# a copy of a copy, and copies that share members
{
    hash<auto!> h = make(10);
    hash<auto!> c1 = h;
    hash<auto!> c2 = c1;
    c2.k0 = "c2";
    hash<auto!> c3 = c2;
    c3.k9 = "c3";
    c1.k5 = "c1";
    show("copies h", \summary(), h);
    show("copies c1", \summary(), c1);
    show("copies c2", \summary(), c2);
    show("copies c3", \summary(), c3);
}

# typed hashes keep their type in the copy
{
    hash<string, int> h = {"a": 1, "b": 2, "c": 3};
    hash<string, int> c = h;
    c.b = 20;
    c.d = 4;
    show("typed copy", string sub () { return sprintf("%s %s", c.fullType(), summary(c)); });
    show("typed original", string sub () { return sprintf("%s %s", h.fullType(), summary(h)); });
    show("typed wrong value", string sub () { auto v = "x"; c.e = v; return summary(c); });
    show("typed sum", string sub () { hash<string, int> s = h + {"e": 5}; return sprintf("%s %s", s.fullType(), summary(s)); });
    show("typed plus object", string sub () { hash<auto!> s = h + new Holder(); return summary(s); });
    show("typed to auto", string sub () { hash<auto!> s = h; s.z = "z"; return sprintf("%s %s", s.fullType(), summary(s)); });
}

# hashdecl hashes keep their type in the copy
{
    hash<HD> h();
    hash<HD> c = h;
    c.a = 10;
    show("hashdecl copy", string sub () { return sprintf("%s %s", c.fullType(), summary(c)); });
    show("hashdecl original", string sub () { return sprintf("%s %s", h.fullType(), summary(h)); });
    show("hashdecl invalid member", string sub () { string k = "zz"; c{k} = 1; return summary(c); });
    hash<HD2> d();
    hash<HD2> d2 = d;
    d2.d += 3;
    show("hashdecl child copy", string sub () { return sprintf("%s %s", d2.fullType(), summary(d2)); });
    show("hashdecl child original", string sub () { return sprintf("%s %s", d.fullType(), summary(d)); });
    show("hashdecl sum", string sub () { hash<auto!> s = h + d; return sprintf("%s %s", s.fullType(), summary(s)); });
    show("hashdecl to auto", string sub () { hash<auto!> s = h; s.b = "changed"; string k = "q"; s{k} = 1; return summary(s); });
}

# hash literals with values computed at run time
{
    int x = 5;
    string s = "five";
    hash<auto!> h = {"x": x, "s": s, "sum": x + 1, "n": {"x": x}, "naïve": s + "!"};
    show("literal", \summary(), h);
    hash<HD> hd = <HD>{"a": x, "b": s};
    show("hashdecl literal", \summary(), hd);
    show("hashdecl literal wrong type", string sub () { hash<HD> e = <HD>{"a": s}; return summary(e); });
    hash<string, int> th = {"a": x, "b": x * 2};
    show("typed literal", string sub () { return sprintf("%s %s", th.fullType(), summary(th)); });
}

# hash operations that build on copies
{
    hash<auto!> h = make(5);
    hash<auto!> c = h + {"k2": "two", "k9": 9};
    show("plus", \summary(), c);
    c += {"k0": "zero", "k10": 10};
    show("plus equals", \summary(), c);
    show("plus original", \summary(), h);
    hash<auto!> s = h{"k1", "k3", "k4"};
    show("slice", \summary(), s);
    show("compare", string sub () { hash<auto> d = h; return sprintf("%y %y", d == h, c == h); });
}

# objects copy their members
{
    # the member names are given at run time
    list<string> names = ("x", "naïve", "extra", "extra2", "h");
    Holder o();
    o{names[1]} = "n";
    o{names[2]} = 1;
    Holder o2 = o.copy();
    o2{names[0]} = 2;
    o2{names[1]} = "changed";
    o2{names[3]} = 2;
    code members = string sub (object obj) {
        return sprintf("%y", (map {$1: obj{$1}}, names));
    };
    show("object copy", string sub () { return members(o2); });
    show("object original", string sub () { return members(o); });
    remove o2{names[2]};
    show("object copy removed", string sub () { return sprintf("%s %s", members(o2), members(o)); });
}

# a copy that is changed while it is iterated
{
    hash<auto!> h = make(6);
    hash<auto!> c = h;
    foreach string k in (keys c) {
        if (k =~ /[13]$/) {
            delete c{k};
        } else {
            c{k + "x"} = 1;
        }
    }
    show("iterated copy", \summary(), c);
    show("iterated original", \summary(), h);
}
