#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# The type of a value stored in an element of a list whose element type is untyped (list, softlist, list<any>) is
# stripped of its complex type, as for any other untyped target (an any variable, a key of an untyped hash), in every
# execution mode (qore#7573: the IR interpreter and native code kept the complex type for a store with a constant
# index into a local list); an element type that is auto keeps it.

%modern

sub show(string label, auto v) {
    printf("%s: %s\n", label, v.fullType());
}

hash<auto> h = {"a": 1};
list<auto> la = (1, 2);

# a global softlist that has no value
our softlist gsl;

#! a class with a softlist member that has no value
class SoftListHolder {
    public {
        softlist sl;
        static softlist ssl;
    }
}

# a new element, an existing element and one past the end
{
    list l = ();
    l[0] = h;
    show("list new", l[0]);
    l[0] = h;
    show("list existing", l[0]);
    l[2] = h;
    show("list past the end", l[2]);
    l[1] = la;
    show("list list value", l[1]);
}
{
    softlist sl = ();
    sl[0] = h;
    show("softlist", sl[0]);
}
# a softlist that has no value is vivified as an untyped list, as a softlist variable's initial list is
{
    softlist sv;
    sv[0] = 1;
    sv[1] = h;
    printf("vivified softlist: %s %s\n", sv.fullType(), sv[1].fullType());
    *softlist osv;
    osv[0] = h;
    printf("vivified *softlist: %s %s\n", osv.fullType(), osv[0].fullType());
}
# softlist targets reached through an lvalue path
{
    gsl[0] = h;
    printf("vivified global softlist: %s %s\n", gsl.fullType(), gsl[0].fullType());
    SoftListHolder slh();
    slh.sl[0] = h;
    printf("vivified member softlist: %s %s\n", slh.sl.fullType(), slh.sl[0].fullType());
    list nested = (1, NOTHING);
    nested[1][0] = h;
    printf("vivified nested list: %s %s\n", nested[1].fullType(), nested[1][0].fullType());
    SoftListHolder::ssl[0] = h;
    printf("vivified static softlist: %s %s\n", SoftListHolder::ssl.fullType(), SoftListHolder::ssl[0].fullType());
    hash<string, softlist> hsl = {};
    hsl.x[0] = h;
    printf("vivified softlist hash value: %s %s\n", hsl.x.fullType(), hsl.x[0].fullType());
    softlist vsl;
    int i = 0;
    vsl[i] = h;
    printf("vivified softlist variable index: %s %s\n", vsl.fullType(), vsl[0].fullType());
}
{
    list<any> lany = ();
    lany[0] = h;
    show("list<any>", lany[0]);
}
{
    list l = (NOTHING,);
    l[0] = h;
    show("list from a literal", l[0]);
}
{
    list l = ();
    code c = sub () { l[0] = h; };
    c();
    show("closure list", l[0]);
}
# an element type that is auto keeps the type of the value
{
    list<auto> lauto = ();
    lauto[0] = h;
    show("list<auto>", lauto[0]);
}
# the same for the other targets that strip it
{
    any a = h;
    show("any", a);
    hash h2 = {};
    h2.x = h;
    show("hash key", h2.x);
    list l = ();
    int i = 0;
    l[i] = h;
    show("list variable index", l[i]);
}
