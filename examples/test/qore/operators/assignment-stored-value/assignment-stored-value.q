#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# prints the value and the type of assignment expressions whose target converts the value assigned (issues #7575
# and #7589: every way that the store of a list element or a hash key takes)

%modern
%allow-weak-references

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

# every way that the store of a hash key or a list element with a constant index takes, with the value used (#7589)
{
    # copy-on-write: the container is shared
    hash<string, softint> hs = {"x": 1};
    hash<string, softint> shared = hs;
    show("copied hash", (hs.x = "2"));
    show("copied hash original", shared.x);
    list<softint> ls = (1,);
    list<softint> lshared = ls;
    show("copied list", (ls[0] = "3"));
    show("copied list original", lshared[0]);
    string k = "x";
    hash<string, softint> hs2 = hs;
    show("copied hash dynamic", (hs{k} = "4"));
    show("copied hash dynamic original", hs2.x);

    # auto-vivification from NOTHING
    *hash<string, softint> vh;
    show("vivified hash", (vh.x = "5"));
    *list<softint> vl;
    show("vivified list", (vl[0] = "6"));
    *hash<string, softint> vh2;
    show("vivified hash dynamic", (vh2{k} = "7"));
    *hash<AsvHash> vhd;
    try {
        show("vivified hashdecl", (vhd.si = "8"));
    } catch (hash<ExceptionInfo> ex) {
        printf("vivified hashdecl: %s\n", ex.err);
    }

    # an element that holds a reference: the assignment is made through it
    softint target;
    hash<auto> hr = {"r": \target};
    show("element reference", (hr.r = "9"));
    show("element reference target", target);
    list<auto> lr = (\target,);
    show("list element reference", (lr[0] = "10"));
    show("list element reference target", target);

    # an untyped container that holds an object
    auto ao = new AsvHolder();
    show("object member through an untyped container", (ao.si = "11"));
    show("object hash member through an untyped container", (ao.hh = h));
    auto ao2 = new AsvHolder();
    show("object member through an untyped container, dynamic", (ao2{"si"} = "12"));

    # values that are not converted
    hash<auto> ha = {};
    show("hash<auto> typed hash", (ha.x = h));
    list<auto> la = ();
    show("list<auto> typed hash", (la[0] = h));
    show("nothing", (ha.n = NOTHING));

    # the value of the assignment is a value of its own
    hash<string, list<int>> hl = {};
    auto r = (hl.x = (1, 2));
    r += 3;
    show("own value stored", hl.x);
    show("own value", r);
}

# a weak reference to a container, written through
{
    hash<string, softint> strong = {"x": 1};
    hash<string, softint> wk := strong;
    show("weak hash", (wk.x = "14"));
    show("weak hash target", strong.x);
    list<softint> lstrong = (1,);
    list<softint> lwk := lstrong;
    show("weak list", (lwk[0] = "15"));
    show("weak list target", lstrong[0]);
}

# in a loop: the value of each iteration is released
{
    hash<string, softint> hs = {};
    list<softint> ls = (0,);
    int t = 0;
    for (int i = 0; i < 5; ++i) {
        t += (hs.x = i.toString());
        t += (ls[0] = i.toString());
    }
    show("loop sum", t);
}

# a list created by the assignment for a declaration that has no value: an untyped declaration takes an untyped list,
# whose element is stored without its complex type, also as the value of the assignment (#7589)
{
    *softlist vsl;
    show("vivified *softlist", (vsl[0] = h));
    show("vivified *softlist stored", vsl[0]);
    *list<any> vla;
    show("vivified *list<any>", (vla[0] = h));
    *list vl;
    show("vivified *list", (vl[0] = h));
    list l0;
    show("vivified list past the end", (l0[2] = h));
    show("vivified list past the end stored", l0[2]);
    auto va;
    show("vivified auto", (va[0] = h));
    *list<auto> vlauto;
    show("vivified *list<auto>", (vlauto[0] = h));
}
