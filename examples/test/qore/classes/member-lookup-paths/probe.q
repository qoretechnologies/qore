#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the probe of member-lookup-paths.qtest: prints the results of member and hash key accesses through each lookup path
# (qore#7194); the names have the lengths at which the comparison of names changes the size of its loads (1, 2, 3, 4,
# 5, 7, 8, 9, 15, 16, 17 and 33 bytes), and names of one length differ in one byte at the start, in the middle or at
# the end, so that a lookup compares a name with names that differ from it only there

%modern

# a class with fewer than 8 members (searched in a list): each lookup of the last name of a length compares it with
# the names before it, which differ in one byte
class Small {
    public {
        int qb = 21;
        int aq = 22;
        int ab = 2;
        int qbc = 31;
        int abq = 32;
        int abc = 3;
        int a = 1;
    }
}

class Small5 {
    public {
        int qbcde = 51;
        int abqde = 52;
        int abcdq = 53;
        int abcde = 5;
        int abcd = 4;
    }
}

class Small9 {
    public {
        int qbcdefghi = 91;
        int aqcdefghi = 92;
        int abcdqfghi = 93;
        int abcdefghq = 94;
        int abcdefghi = 9;
        int abcdefgh = 8;
        int abcdefg = 7;
    }
}

class Small17 {
    public {
        int qbcdefghijklmnopq = 171;
        int abcdefghqjklmnopq = 172;
        int abcdefghijklqnopq = 173;
        int abcdefghijklmnopz = 174;
        int abcdefghijklmnopq = 17;
        int abcdefghijklmnop = 16;
        int abcdefghijklmno = 15;
    }
}

# a class with 8 members or more (searched in an index)
class Big {
    public {
        int a = 1;
        int ab = 2;
        int abc = 3;
        int abcd = 4;
        int abcde = 5;
        int abcdefg = 7;
        int abcdefgh = 8;
        int abcdefghi = 9;
        int abcdefghijklmno = 15;
        int abcdefghijklmnop = 16;
        int abcdefghijklmnopq = 17;
        int abcdefghijklmnopqrstuvwxyz0123456 = 33;
        int abcdefghijklmnopqrstuvwxyz0123457 = 34;
    }
}

# private members read in the class's methods, and a base class's private member read on an object of a subclass
class PBase {
    private {
        int p = 10;
        int pp = 11;
    }

    public {
        int x = 1;
    }

    int readTyped(PBase o) {
        return o.p + o.pp;
    }

    auto readAny(object o) {
        return o.p;
    }

    auto readName(object o, string n) {
        return o{n};
    }

    writeTyped(PBase o, int v) {
        o.pp = v;
    }
}

class PSub inherits PBase {
    public {
        int y = 2;
    }

    auto readOwn(object o) {
        return o.y;
    }
}

# a private:internal member of the same name in a class and its base class: each class's code reads its own
class IBase {
    private:internal {
        int v = 100;
    }

    auto baseV(object o) {
        return o.v;
    }

    baseSet(object o, int n) {
        o.v = n;
    }
}

class ISub inherits IBase {
    private:internal {
        int v = 200;
    }

    auto subV(object o) {
        return o.v;
    }
}

# a class inherited with private:internal: its members are used only by its own code and the inheriting class's
class IHost inherits private:internal PBase {
    auto hostX(object o) {
        return o.x;
    }
}

# a class that declares no members: its members are added and removed at run time
class Dyn {
}

# a class with a member gate
class MGate {
    public {
        int d = 1;
        *int n;
    }

    private {
        int hidden = 2;
    }

    auto memberGate(string m) {
        return "gate:" + m;
    }
}

hashdecl HD {
    int qbc = 31;
    int abq = 32;
    int abc = 3;
    int abcdefghijklmnopq = 17;
}

sub show(string label, code c) {
    try {
        auto v = c();
        printf("%s: %y\n", label, v);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

sub check_object(string label, object o, list<string> names) {
    foreach string n in (names) {
        show(sprintf("%s{%s}", label, n), auto sub () { return o{n}; });
        # the name in another encoding
        show(sprintf("%s{%s} iso-8859-1", label, n), auto sub () {
            return o{convert_encoding(n, "ISO-8859-1")};
        });
    }
}

sub check_hash(string label, hash<auto> h, list<string> key_list) {
    foreach string k in (key_list) {
        show(sprintf("%s{%y}", label, k), auto sub () { return h{k}; });
        show(sprintf("%s{%y} exists", label, k), auto sub () { return exists h{k}; });
        show(sprintf("%s{%y} iso-8859-1", label, k), auto sub () {
            return h{convert_encoding(k, "ISO-8859-1")};
        });
        show(sprintf("%s{%y} utf-16le", label, k), auto sub () { return h{convert_encoding(k, "UTF-16LE")}; });
    }
}

# members through typed variables, with constant names
{
    Small s();
    printf("small: %y %y %y %y %y %y %y\n", s.qb, s.aq, s.ab, s.qbc, s.abq, s.abc, s.a);
    Small5 s5();
    printf("small5: %y %y %y %y %y\n", s5.qbcde, s5.abqde, s5.abcdq, s5.abcde, s5.abcd);
    Small9 s9();
    printf("small9: %y %y %y %y %y %y %y\n", s9.qbcdefghi, s9.aqcdefghi, s9.abcdqfghi, s9.abcdefghq, s9.abcdefghi,
        s9.abcdefgh, s9.abcdefg);
    Small17 s17();
    printf("small17: %y %y %y %y %y %y %y\n", s17.qbcdefghijklmnopq, s17.abcdefghqjklmnopq, s17.abcdefghijklqnopq,
        s17.abcdefghijklmnopz, s17.abcdefghijklmnopq, s17.abcdefghijklmnop, s17.abcdefghijklmno);
    Big b();
    printf("big: %y %y %y %y %y %y %y %y %y %y %y %y %y\n", b.a, b.ab, b.abc, b.abcd, b.abcde, b.abcdefg, b.abcdefgh,
        b.abcdefghi, b.abcdefghijklmno, b.abcdefghijklmnop, b.abcdefghijklmnopq, b.abcdefghijklmnopqrstuvwxyz0123456,
        b.abcdefghijklmnopqrstuvwxyz0123457);

    # writes
    s.abc = -3;
    s.abq = -32;
    s9.abcdefghi = -9;
    s9.abcdefghq = -94;
    s17.abcdefghijklmnopq = -17;
    s17.abcdefghijklmnopz = -174;
    b.abcdefghijklmnopqrstuvwxyz0123457 = -34;
    b.abcdefgh = -8;
    printf("written: %y %y %y %y %y %y %y %y\n", s.abc, s.abq, s9.abcdefghi, s9.abcdefghq, s17.abcdefghijklmnopq,
        s17.abcdefghijklmnopz, b.abcdefghijklmnopqrstuvwxyz0123457, b.abcdefgh);
    printf("written untouched: %y %y %y %y\n", s.qbc, s9.qbcdefghi, s17.abcdefghijklqnopq,
        b.abcdefghijklmnopqrstuvwxyz0123456);
}

# members through untyped variables and with names given at run time
{
    auto s = new Small();
    auto s9 = new Small9();
    auto s17 = new Small17();
    auto b = new Big();
    printf("untyped: %y %y %y %y %y %y\n", s.abc, s.abq, s9.abcdefghq, s17.abcdefghijklmnopz, b.abcdefghijklmnopq,
        b.abcdefghijklmnopqrstuvwxyz0123456);
    s.ab = -2;
    s17.abcdefghijklqnopq = -173;
    b.abcde = -5;
    printf("untyped written: %y %y %y\n", s.ab, s17.abcdefghijklqnopq, b.abcde);
    check_object("small", s, ("a", "ab", "aq", "qb", "abc", "abd", "ab ", "b", ""));
    check_object("small9", s9, ("abcdefghi", "abcdefghq", "abcdefghz", "zbcdefghi", "abcdefgh", "abcdefghij"));
    check_object("small17", s17, ("abcdefghijklmnopq", "abcdefghijklmnopz", "abcdefghijklmnopy",
        "abcdefghijklmnop", "abcdefghijklmnopqr"));
    check_object("big", b, ("abcd", "abce", "abcdefghijklmnopqrstuvwxyz0123456", "abcdefghijklmnopqrstuvwxyz0123457",
        "abcdefghijklmnopqrstuvwxyz0123458", "abcdefghijklmnopqrstuvwxyz012345"));
    show("invalid write", auto sub () { s.abd = 1; });
    show("invalid write runtime name", auto sub () { string n = "abcdefghz"; s9{n} = 1; });
}

# private members
{
    PBase pb();
    PSub ps();
    printf("private typed: %y\n", pb.readTyped(pb));
    printf("private any: %y %y\n", pb.readAny(pb), pb.readAny(ps));
    printf("private name: %y %y %y\n", pb.readName(ps, "p"), pb.readName(ps, "pp"), pb.readName(ps, "y"));
    printf("subclass own: %y\n", ps.readOwn(ps));
    pb.writeTyped(ps, 12);
    printf("private written: %y\n", pb.readTyped(ps));
    show("private outside", auto sub () { auto o = pb; return o.p; });
    show("private outside subclass", auto sub () { auto o = ps; return o.pp; });
    show("private outside name", auto sub () { string n = "pp"; return ps{n}; });
    show("public inherited", auto sub () { auto o = ps; return o.x + o.y; });
}

# private:internal members
{
    ISub is();
    printf("internal: %y %y\n", is.baseV(is), is.subV(is));
    is.baseSet(is, 101);
    printf("internal written: %y %y\n", is.baseV(is), is.subV(is));
    IBase ib();
    printf("internal base: %y\n", ib.baseV(ib));
    show("internal outside", auto sub () { auto o = is; return o.v; });
    IHost ih();
    printf("internal host: %y\n", ih.hostX(ih));
    show("internal host outside", auto sub () { auto o = ih; return o.x; });
}

# members added and removed at run time
{
    object d = new Dyn();
    d.a = 1;
    d.abcdefghi = 9;
    d{"abcdefghijklmnopq"} = 17;
    d{"ключ"} = "cyrillic";
    d{"日本語キー"} = "japanese";
    d{"naïve"} = "latin";
    printf("dyn keys: %y\n", keys d);
    check_object("dyn", d, ("a", "b", "abcdefghi", "abcdefghq", "abcdefghijklmnopq", "abcdefghijklmnopz", "ключ",
        "ключи", "日本語キー", "日本語キ", "naïve", "naive"));
    delete d.abcdefghi;
    printf("dyn deleted: %y %y\n", d.abcdefghi, keys d);
    auto r = remove d{"ключ"};
    printf("dyn removed: %y %y %y\n", r, d{"ключ"}, keys d);
    d.abcdefghi = -9;
    d{"ключ"} = "again";
    printf("dyn re-added: %y %y %y\n", d.abcdefghi, d{"ключ"}, keys d);
    # a key in another encoding names the same member
    d{convert_encoding("naïve", "ISO-8859-1")} = "converted";
    printf("dyn encoding: %y %y\n", d{"naïve"}, keys d);
    show("dyn nul", auto sub () { return d{"a\x00b"}; });
}

# a member gate
{
    MGate g();
    auto o = g;
    printf("gate: %y %y %y %y\n", o.d, o.n, o.zz, o.hidden);
    delete o.d;
    printf("gate deleted: %y\n", o.d);
    o.d = 3;
    printf("gate re-set: %y %y\n", o.d, o{"abcdefghijklmnopq"});
}

# a deleted object
{
    Small s();
    auto o = s;
    delete s;
    show("deleted object", auto sub () { return o.abc; });
}

# hash keys
{
    hash<auto> h = {
        "": "empty",
        "a": 1,
        "ab": 2,
        "aq": 22,
        "abc": 3,
        "abcd": 4,
        "abcde": 5,
        "abcdefg": 7,
        "abcdefgh": 8,
        "abcdefghi": 9,
        "abcdefghq": 99,
        "abcdefghijklmno": 15,
        "abcdefghijklmnop": 16,
        "abcdefghijklmnopq": 17,
        "abcdefghijklmnopz": 177,
        "abcdefghijklmnopqrstuvwxyz0123456": 33,
        "ключ": "cyrillic",
        "日本語キー": "japanese",
        "naïve": "latin",
    };
    check_hash("hash", h, ("", "a", "b", "ab", "aq", "abc", "abd", "abcdefghi", "abcdefghq", "abcdefghz",
        "abcdefghijklmnopq", "abcdefghijklmnopz", "abcdefghijklmnopy", "abcdefghijklmnopqrstuvwxyz0123456",
        "abcdefghijklmnopqrstuvwxyz0123457", "ключ", "ключи", "日本語キー", "日本語キ", "naïve", "naive"));
    printf("hash members: %y %y %y %y\n", h.a, h.abcdefghq, h.abcdefghijklmnopz, h.abcdefghijklmnopqrstuvwxyz0123456);
    h.abcdefghq = -99;
    h{"ключ"} = "changed";
    h{convert_encoding("naïve", "ISO-8859-1")} = "converted";
    h.new_key_17_bytes_ = "new";
    printf("hash written: %y %y %y %y %y\n", h.abcdefghq, h{"ключ"}, h{"naïve"}, h.new_key_17_bytes_, h.size());
    delete h.abcdefghi;
    auto r = remove h{"日本語キー"};
    printf("hash removed: %y %y %y %y\n", h.abcdefghi, r, exists h{"日本語キー"}, h.size());
    h.abcdefghi = -9;
    printf("hash re-added: %y %y\n", h.abcdefghi, h.lastKey());
    show("hash nul", auto sub () { return h{"a\x00b"}; });

    # hash comparisons: the same keys in another order, and a key that differs in its last byte
    hash<auto> h1 = {"abcdefghijklmnopq": 1, "ab": 2, "ключ": 3};
    hash<auto> h2 = {"ключ": 3, "abcdefghijklmnopq": 1, "ab": 2};
    hash<auto> h3 = {"abcdefghijklmnopz": 1, "ab": 2, "ключ": 3};
    printf("hash compare: %y %y %y %y\n", h1 == h2, h1 == h3, h1 === h2, h1 != h3);
    printf("hash minus: %y\n", keys (h1 - ("ab", "abcdefghijklmnopz")));
    printf("hash slice: %y\n", h1{"ab", "abcdefghijklmnopq", "abcdefghijklmnopz"});
}

# hashdecl members
{
    hash<HD> hd();
    printf("hashdecl: %y %y %y %y\n", hd.qbc, hd.abq, hd.abc, hd.abcdefghijklmnopq);
    hd.abc = -3;
    printf("hashdecl written: %y %y\n", hd.abc, hd.abq);
    show("hashdecl invalid", auto sub () { string n = "abd"; hd{n} = 1; });
    show("hashdecl invalid long", auto sub () { string n = "abcdefghijklmnopz"; hd{n} = 1; });
}
