#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# a value made from a list or hash freed under a weak reference with another type (a cast, or the value given to a
# variable, parameter, member, list element or return value of another type) and a new expression made from it are new
# containers, which can be changed; a value that stays the same container (a cast to the identical type or to its
# or-nothing type, auto, hash<auto>, list<auto>, *list<int> for a list<int>) keeps the freed state and cannot be
# changed, in every mode (qore#7199); see ../weak-reference-freed-conversions.qtest

%modern
%allow-weak-references
%no-child-restrictions

hashdecl HD {
    int a = 1;
}

hashdecl HD2 {
    int a = 2;
    int b = 3;
}

sub try_hash(string label, hash<auto> h) {
    try {
        h.a = 5;
        printf("%s: changed %y type %s\n", label, h, h.fullType());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s type %s\n", label, ex.err, h.fullType());
    }
}

sub try_list(string label, list<auto> l) {
    try {
        l += 9;
        printf("%s: changed %y type %s\n", label, l, l.fullType());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s type %s\n", label, ex.err, l.fullType());
    }
}

sub param_plain_hash(hash h) {
    try_hash("param hash", h);
}

sub param_soft_hash(hash<string, softint> h) {
    try_hash("param hash<string, softint>", h);
}

sub param_auto_hash(hash<auto> h) {
    try_hash("param hash<auto>", h);
}

sub param_plain_list(list l) {
    try_list("param list", l);
}

sub param_soft_list(list<softint> l) {
    try_list("param list<softint>", l);
}

sub param_auto_list(list<auto> l) {
    try_list("param list<auto>", l);
}

class Holder {
    public {
        hash hm;
        hash<auto> ham;
        list<softint> lm;
    }
}

hash sub ret_plain_hash(hash<string, int> h) {
    return h;
}

list<softint> sub ret_soft_list(list<int> l) {
    return l;
}

list<auto> sub ret_auto_list(list<int> l) {
    return l;
}

sub test(string kind) {
    hash<string, int> wh;
    hash<HD> wd;
    list<int> wl;
    hash<auto!> wph;
    list<auto!> wpl;
    hash<string, int> hs = {"a": 1};
    hash<HD> d();
    list<int> ls = (1, 2);
    hash<auto!> phs = {"a": "1"};
    list<auto!> pls = ("1", 2);
    wh := hs;
    wd := d;
    wl := ls;
    wph := phs;
    wpl := pls;
    if (kind == "freed") {
        remove hs;
        remove d;
        remove ls;
        remove phs;
        remove pls;
    }
    printf("== %s\n", kind);

    # casts to another type
    try_hash("cast<hash>", cast<hash>(wh));
    try_hash("cast<hash<string, softint>>", cast<hash<string, softint>>(wh));
    try_hash("cast<hash<HD2>> of hash<HD>", cast<hash<HD2>>(wd));
    try_hash("cast<hash<HD>> of hash<string, int>", cast<hash<HD>>(wh));
    try_list("cast<list>", cast<list>(wl));
    try_list("cast<list<softint>>", cast<list<softint>>(wl));
    # casts to the identical type
    try_hash("cast<hash<string, int>>", cast<hash<string, int>>(wh));
    try_hash("cast<hash<HD>> of hash<HD>", cast<hash<HD>>(wd));
    try_list("cast<list<int>>", cast<list<int>>(wl));
    # or-nothing casts: the or-nothing type of the value's own type (qore#6174) is the same type
    try_hash("cast<*hash<string, int>>", cast<*hash<string, int>>(wh));
    try_hash("cast<*hash<HD>> of hash<HD>", cast<*hash<HD>>(wd));
    try_list("cast<*list<int>>", cast<*list<int>>(wl));
    try_list("cast<*list<softint>>", cast<*list<softint>>(wl));
    # plain containers and hashdecl hashes converted to typed ones
    try_hash("cast<hash<string, softint>> of plain hash", cast<hash<string, softint>>(wph));
    try_list("cast<list<softint>> of plain list", cast<list<softint>>(wpl));
    try_hash("cast<hash> of hash<HD>", cast<hash>(wd));
    try_hash("cast<hash<string, int>> of hash<HD>", cast<hash<string, int>>(wd));
    # new expressions make a new container
    try_hash("new hash<string, int>", new hash<string, int>(wh));
    try_hash("new hash<string, softint>", new hash<string, softint>(wh));
    try_hash("new hash<HD>", new hash<HD>(wd));

    # variables of another type
    hash h1 = wh;
    try_hash("hash var", h1);
    hash<string, softint> h2 = wh;
    try_hash("hash<string, softint> var", h2);
    list l1 = wl;
    try_list("list var", l1);
    list<softint> l2 = wl;
    try_list("list<softint> var", l2);
    softlist<softint> l3 = wl;
    try_list("softlist<softint> var", l3);
    # variables that hold the same container
    hash<auto> h3 = wh;
    try_hash("hash<auto> var", h3);
    auto h4 = wh;
    try_hash("auto var (hash)", h4);
    list<auto> l4 = wl;
    try_list("list<auto> var", l4);
    *list<int> l5 = wl;
    try_list("*list<int> var", l5);
    hash<string, softint> h5 = wph;
    try_hash("hash<string, softint> var of plain hash", h5);
    list<softint> l6 = wpl;
    try_list("list<softint> var of plain list", l6);

    # return values, members and elements of another type
    try_hash("return hash", ret_plain_hash(wh));
    try_list("return list<softint>", ret_soft_list(wl));
    try_list("return list<auto>", ret_auto_list(wl));
    Holder o();
    o.hm = wh;
    try_hash("member hash", o.hm);
    o.ham = wh;
    try_hash("member hash<auto>", o.ham);
    o.lm = wl;
    try_list("member list<softint>", o.lm);
    list<list<softint>> ll = ();
    ll[0] = wl;
    try_list("element list<softint>", ll[0]);

    # parameters
    param_plain_hash(wh);
    param_soft_hash(wh);
    param_auto_hash(wh);
    param_plain_list(wl);
    param_soft_list(wl);
    param_auto_list(wl);

    # the container of the weak reference itself
    try {
        wh.y = 1;
        printf("weak hash: changed %y\n", wh);
    } catch (hash<ExceptionInfo> ex) {
        printf("weak hash: %s\n", ex.err);
    }
    # the container of the weak reference itself stays as it is after the conversions
    try {
        wl[0] = 7;
        printf("weak list: changed %y\n", wl);
    } catch (hash<ExceptionInfo> ex) {
        printf("weak list: %s\n", ex.err);
    }
    try {
        wd.a = 7;
        printf("weak hashdecl hash: changed %y\n", wd);
    } catch (hash<ExceptionInfo> ex) {
        printf("weak hashdecl hash: %s\n", ex.err);
    }
}

test("live");
test("freed");
