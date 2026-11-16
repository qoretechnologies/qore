#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# applies post-increments and post-decrements whose value is unused to lvalues whose type refuses the result; run by
# unused-post-incdec-errors.qtest

/** Each check applies x++ or x-- as an expression statement (or as the update of a for loop) to an lvalue whose type
    refuses the result, so the operator raises RUNTIME-TYPE-ERROR, whose description names the post operator
    ("<++ (post) operator>"), and leaves the lvalue unchanged: enum lvalues of each kind (a local, a closure-bound
    local, a global, an object member, a hashdecl member, a hash element and a list element), and untyped lvalues
    whose value's container gives them a type that is not numeric (a member of a hashdecl or an object read through
    a variable of no type, an element of a typed list).  The same operators on number, int, float and untyped lvalues
    are controls: they raise nothing and change the lvalue.  Each check prints
    "name: <error>: <description> -> <type> <value>", or "name: none -> <type> <value>".
*/

%modern
%enable-all-warnings

enum EI : int {
    One = 1,
    Seven = 7,
}

enum EF : float {
    Half = 0.5,
}

enum EN : number {
    OneHalf = 1.5n,
}

hashdecl HD {
    EI ei = EI::Seven;
    EF ef = EF::Half;
    EN en = EN::OneHalf;
    string s = "x";
    bool b = True;
    date d = 2026-01-01;
    binary bin = <01>;
    list<int> l = (1,);
    number n = 1.5n;
    float f = 1.5;
    int i = 2;
}

class Holder {
    public {
        EI ei = EI::Seven;
        EF ef = EF::Half;
        EN en = EN::OneHalf;
        string s = "x";
        bool b = True;
        date d = 2026-01-01;
        number n = 1.5n;
    }

    #! applies the operators to members named without and with "self."
    selfMembers() {
        try { ei++; r("self-member-ei-inc", ei); } catch (hash<ExceptionInfo> ex) { r("self-member-ei-inc", ei, ex); }
        try { self.ef--; r("self-member-ef-dec", ef); } catch (hash<ExceptionInfo> ex) {
            r("self-member-ef-dec", ef, ex);
        }
        try { en--; r("self-member-en-dec", en); } catch (hash<ExceptionInfo> ex) { r("self-member-en-dec", en, ex); }
        n++;
        r("control-self-member-number", n);
    }
}

our EI g_ei = EI::Seven;
our EF g_ef = EF::Half;
our EN g_en = EN::OneHalf;

#! returns the type and value of v
string sub show(auto v) {
    # a date is printed without its time zone, which depends on the machine
    if (v.typeCode() == NT_DATE) {
        return sprintf("%s %s", v.fullType(), v.format("YYYY-MM-DD"));
    }
    return sprintf("%s %y", v.fullType(), v);
}

#! prints the result of a check
sub r(string name, auto v, *hash<ExceptionInfo> ex) {
    if (ex) {
        printf("%s: %s: %s -> %s\n", name, ex.err, ex.desc, show(v));
    } else {
        printf("%s: none -> %s\n", name, show(v));
    }
}

# enum locals
sub enum_locals() {
    {
        EI v = EI::Seven;
        try { v++; r("local-ei-inc", v); } catch (hash<ExceptionInfo> ex) { r("local-ei-inc", v, ex); }
        try { v--; r("local-ei-dec", v); } catch (hash<ExceptionInfo> ex) { r("local-ei-dec", v, ex); }
    }
    {
        EF v = EF::Half;
        try { v++; r("local-ef-inc", v); } catch (hash<ExceptionInfo> ex) { r("local-ef-inc", v, ex); }
        try { v--; r("local-ef-dec", v); } catch (hash<ExceptionInfo> ex) { r("local-ef-dec", v, ex); }
    }
    {
        EN v = EN::OneHalf;
        try { v++; r("local-en-inc", v); } catch (hash<ExceptionInfo> ex) { r("local-en-inc", v, ex); }
        try { v--; r("local-en-dec", v); } catch (hash<ExceptionInfo> ex) { r("local-en-dec", v, ex); }
    }
    {
        EI v = EI::Seven;
        code get = auto sub () { return v; };
        try { v++; r("closure-ei-inc", get()); } catch (hash<ExceptionInfo> ex) {
            r("closure-ei-inc", get(), ex);
        }
        try { v--; r("closure-ei-dec", get()); } catch (hash<ExceptionInfo> ex) {
            r("closure-ei-dec", get(), ex);
        }
    }
    {
        EN v = EN::OneHalf;
        code get = auto sub () { return v; };
        try { v++; r("closure-en-inc", get()); } catch (hash<ExceptionInfo> ex) {
            r("closure-en-inc", get(), ex);
        }
        try { v--; r("closure-en-dec", get()); } catch (hash<ExceptionInfo> ex) {
            r("closure-en-dec", get(), ex);
        }
    }
    # the update of a for loop
    {
        EI v = EI::Seven;
        try {
            for (int i = 0; i < 2; v++) {
                ++i;
            }
            r("for-ei-inc", v);
        } catch (hash<ExceptionInfo> ex) {
            r("for-ei-inc", v, ex);
        }
        try {
            for (int i = 0; i < 2; v--) {
                ++i;
            }
            r("for-ei-dec", v);
        } catch (hash<ExceptionInfo> ex) {
            r("for-ei-dec", v, ex);
        }
    }
}

# enum globals
sub enum_globals() {
    try { g_ei++; r("global-ei-inc", g_ei); } catch (hash<ExceptionInfo> ex) { r("global-ei-inc", g_ei, ex); }
    try { g_ei--; r("global-ei-dec", g_ei); } catch (hash<ExceptionInfo> ex) { r("global-ei-dec", g_ei, ex); }
    try { g_ef++; r("global-ef-inc", g_ef); } catch (hash<ExceptionInfo> ex) { r("global-ef-inc", g_ef, ex); }
    try { g_ef--; r("global-ef-dec", g_ef); } catch (hash<ExceptionInfo> ex) { r("global-ef-dec", g_ef, ex); }
    try { g_en++; r("global-en-inc", g_en); } catch (hash<ExceptionInfo> ex) { r("global-en-inc", g_en, ex); }
    try { g_en--; r("global-en-dec", g_en); } catch (hash<ExceptionInfo> ex) { r("global-en-dec", g_en, ex); }
}

# enum members and elements
sub enum_containers() {
    Holder o();
    try { o.ei++; r("member-ei-inc", o.ei); } catch (hash<ExceptionInfo> ex) { r("member-ei-inc", o.ei, ex); }
    try { o.ef--; r("member-ef-dec", o.ef); } catch (hash<ExceptionInfo> ex) { r("member-ef-dec", o.ef, ex); }
    try { o.en++; r("member-en-inc", o.en); } catch (hash<ExceptionInfo> ex) { r("member-en-inc", o.en, ex); }
    hash<HD> h();
    try { h.ei--; r("hashdecl-ei-dec", h.ei); } catch (hash<ExceptionInfo> ex) { r("hashdecl-ei-dec", h.ei, ex); }
    try { h.ef++; r("hashdecl-ef-inc", h.ef); } catch (hash<ExceptionInfo> ex) { r("hashdecl-ef-inc", h.ef, ex); }
    try { h.en--; r("hashdecl-en-dec", h.en); } catch (hash<ExceptionInfo> ex) { r("hashdecl-en-dec", h.en, ex); }
    hash<string, EI> he = {"k": EI::Seven};
    string key = "k";
    try { he{key}++; r("hash-ei-inc", he{key}); } catch (hash<ExceptionInfo> ex) { r("hash-ei-inc", he{key}, ex); }
    try { he{key}--; r("hash-ei-dec", he{key}); } catch (hash<ExceptionInfo> ex) { r("hash-ei-dec", he{key}, ex); }
    list<EN> l = (EN::OneHalf,);
    try { l[0]++; r("list-en-inc", l[0]); } catch (hash<ExceptionInfo> ex) { r("list-en-inc", l[0], ex); }
    try { l[0]--; r("list-en-dec", l[0]); } catch (hash<ExceptionInfo> ex) { r("list-en-dec", l[0], ex); }
}

# untyped lvalues whose container gives them a type that refuses the result; the number, float and int members are
# controls
sub untyped_containers() {
    foreach string k in ("ei", "ef", "en", "s", "b", "d", "bin", "l", "n", "f", "i") {
        auto h = new hash<HD>();
        try { h{k}++; r("untyped-hash-" + k + "-inc", h{k}); } catch (hash<ExceptionInfo> ex) {
            r("untyped-hash-" + k + "-inc", h{k}, ex);
        }
        try { h{k}--; r("untyped-hash-" + k + "-dec", h{k}); } catch (hash<ExceptionInfo> ex) {
            r("untyped-hash-" + k + "-dec", h{k}, ex);
        }
    }
    {
        auto h = new hash<HD>();
        try { h.s++; r("untyped-hash-const-s-inc", h.s); } catch (hash<ExceptionInfo> ex) {
            r("untyped-hash-const-s-inc", h.s, ex);
        }
        try { h.ei--; r("untyped-hash-const-ei-dec", h.ei); } catch (hash<ExceptionInfo> ex) {
            r("untyped-hash-const-ei-dec", h.ei, ex);
        }
    }
    foreach string k in ("ei", "ef", "en", "s", "b", "d", "n") {
        object o = new Holder();
        try { o{k}++; r("untyped-member-" + k + "-inc", o{k}); } catch (hash<ExceptionInfo> ex) {
            r("untyped-member-" + k + "-inc", o{k}, ex);
        }
        try { o{k}--; r("untyped-member-" + k + "-dec", o{k}); } catch (hash<ExceptionInfo> ex) {
            r("untyped-member-" + k + "-dec", o{k}, ex);
        }
    }
    {
        object o = new Holder();
        try { o.s--; r("untyped-member-const-s-dec", o.s); } catch (hash<ExceptionInfo> ex) {
            r("untyped-member-const-s-dec", o.s, ex);
        }
        try { o.en++; r("untyped-member-const-en-inc", o.en); } catch (hash<ExceptionInfo> ex) {
            r("untyped-member-const-en-inc", o.en, ex);
        }
    }
    {
        auto l = cast<list<string>>(("x",));
        try { l[0]++; r("untyped-list-s-inc", l[0]); } catch (hash<ExceptionInfo> ex) {
            r("untyped-list-s-inc", l[0], ex);
        }
        auto le = cast<list<EI>>((EI::Seven,));
        try { le[0]--; r("untyped-list-ei-dec", le[0]); } catch (hash<ExceptionInfo> ex) {
            r("untyped-list-ei-dec", le[0], ex);
        }
    }
}

sub ref_inc(auto r) {
    r++;
}

sub ref_dec(auto r) {
    r--;
}

# untyped lvalues that refer to a typed lvalue
sub references() {
    {
        string s = "x";
        try { ref_inc(\s); r("ref-s-inc", s); } catch (hash<ExceptionInfo> ex) { r("ref-s-inc", s, ex); }
        EI e = EI::Seven;
        try { ref_dec(\e); r("ref-ei-dec", e); } catch (hash<ExceptionInfo> ex) { r("ref-ei-dec", e, ex); }
        EN en = EN::OneHalf;
        try { ref_inc(\en); r("ref-en-inc", en); } catch (hash<ExceptionInfo> ex) { r("ref-en-inc", en, ex); }
        number n = 1.5n;
        try { ref_dec(\n); r("ref-n-dec", n); } catch (hash<ExceptionInfo> ex) { r("ref-n-dec", n, ex); }
    }
}

# controls: number, int, float and untyped lvalues accept the result
sub controls() {
    number n = 1.5n;
    n++;
    r("control-number-inc", n);
    n--;
    n--;
    r("control-number-dec", n);
    *number on;
    on++;
    r("control-or-nothing-number-inc", on);
    int i = 7;
    i++;
    i--;
    i--;
    r("control-int", i);
    float f = 0.5;
    f++;
    f--;
    f--;
    r("control-float", f);
    auto a = 1.5n;
    a++;
    r("control-untyped-number", a);
    a = 3;
    a--;
    r("control-untyped-int", a);
    hash<string, number> hn = {"a": 1.5n};
    hn.a++;
    hn.a--;
    hn.a--;
    r("control-hash-number", hn.a);
}

enum_locals();
enum_globals();
{
    Holder h();
    h.selfMembers();
}
enum_containers();
untyped_containers();
references();
controls();

# top-level code
{
    EI v = EI::Seven;
    try { v++; r("toplevel-ei-inc", v); } catch (hash<ExceptionInfo> ex) { r("toplevel-ei-inc", v, ex); }
    try { g_en--; r("toplevel-global-en-dec", g_en); } catch (hash<ExceptionInfo> ex) {
        r("toplevel-global-en-dec", g_en, ex);
    }
}
