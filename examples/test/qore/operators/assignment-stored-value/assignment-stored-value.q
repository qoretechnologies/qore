#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# prints the value and the type of assignment expressions whose target converts the value assigned (issue #7575)

%modern

hash<auto> h = {"a": 1};

hashdecl AsvHash {
    softint si;
    hash hh;
}

class AsvHolder {
    public {
        softint si;
        hash hh;
        static softint ssi;
    }
}

our softint gsi;
our hash gh;

sub show(string label, auto v) {
    printf("%s: %y %s\n", label, v, v.fullType());
}

auto sub pass(auto v) {
    return v;
}

softint sub ret_hash_key(hash<string, softint> hsi) {
    return hsi.r = "21";
}

auto sub ret_list_elem(list<softint> lsi) {
    return lsi[0] = "22";
}

# plain assignments whose value is used
{
    softint si;
    show("local softint", (si = "5"));
    hash hv;
    show("local hash", (hv = h));
    show("global softint", (gsi = "6"));
    show("global hash", (gh = h));
    list l = ();
    show("list element", (l[0] = h));
    show("list element stored", l[0]);
    int i = 1;
    show("list element variable index", (l[i] = h));
    list<softint> lsi = ();
    show("list<softint> element", (lsi[0] = "50"));
    show("list<softint> element stored", lsi[0]);
    list<*softint> lps = ();
    show("list<*softint> element past the end", (lps[3] = "53"));
    show("list<softint> element variable index", (lsi[i] = "51"));
    hash hh = {};
    show("hash key", (hh.x = h));
    show("hash key stored", hh.x);
    string k = "y";
    show("hash key dynamic", (hh{k} = h));
    show("hash key dynamic stored", hh{k});
    hash<string, softint> hsi = {};
    show("hash<string, softint> key", (hsi.x = "7"));
    show("hash<string, softint> key stored", hsi.x);
    show("hash<string, softint> key dynamic", (hsi{k} = "17"));
    hash<AsvHash> hd = <AsvHash>{};
    show("hashdecl softint member", (hd.si = "8"));
    show("hashdecl hash member", (hd.hh = h));
    AsvHolder c();
    show("object softint member", (c.si = "9"));
    show("object hash member", (c.hh = h));
    show("static softint", (AsvHolder::ssi = "10"));
    softint rsi;
    reference<softint> r = \rsi;
    show("reference softint", (r = "11"));
    list nl = (NOTHING, ());
    show("nested list", (nl[1][0] = h));
    softint s2;
    show("chained", (s2 = (hsi.c = "13")));
    show("chained stored", s2);
}

# the value used as an argument, a return value, a condition and in an operator
{
    hash<string, softint> hsi = {};
    show("argument", pass(hsi.a = "31"));
    show("return hash key", ret_hash_key({}));
    show("return list element", ret_list_elem(()));
    if (hsi.z = "0") {
        show("condition", "true");
    } else {
        show("condition", "false");
    }
    list<softint> lsi = ();
    show("operator", (lsi[0] = "40") + 1);
    show("string operator", (hsi.s = "2") + "x");
}

# compound assignments (already the value stored in every mode)
{
    hash<string, softint> hsi = {"x": 1};
    show("hash<string, softint> +=", (hsi.x += 2.5));
    string k = "x";
    show("hash<string, softint> dynamic +=", (hsi{k} += 2.5));
    list<softint> lsi = (1,);
    show("list<softint> +=", (lsi[0] += 2.5));
    hash<string, softstring> hss = {"x": "a"};
    show("hash<string, softstring> +=", (hss.x += 5));
}

# the value is not used: the element is stored and converted as when it is used
{
    hash<string, softint> hsi = {};
    hsi.x = "60";
    show("statement hash key stored", hsi.x);
    list<softint> lsi = ();
    lsi[0] = "61";
    show("statement list element stored", lsi[0]);
}

# a conversion that fails raises its error whether the value is used or not
{
    auto bad = "a";
    hash<string, int> hi = {};
    try {
        show("refused used", (hi.x = bad));
    } catch (hash<ExceptionInfo> ex) {
        printf("refused used: %s\n", ex.err);
    }
    try {
        hi.x = bad;
    } catch (hash<ExceptionInfo> ex) {
        printf("refused unused: %s\n", ex.err);
    }
    list<int> li = ();
    try {
        show("refused list used", (li[0] = bad));
    } catch (hash<ExceptionInfo> ex) {
        printf("refused list used: %s\n", ex.err);
    }
    show("refused stored", hi.x);
    show("refused list stored", li[0]);
}
