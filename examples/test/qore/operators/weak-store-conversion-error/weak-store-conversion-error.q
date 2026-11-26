#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# weak (:=) and opaque (@=) assignments of containers that the lvalue's type converts to a copy of their own, with
# types known only at run time: each is refused with RUNTIME-TYPE-ERROR and leaves the lvalue as it was, while the
# lvalues that keep the value store a reference to it (issue #7572)

%modern
%allow-weak-references
%allow-opaque-references

hashdecl WscHash {
    int a = 1;
}

class WscHolder {
    public {
        hash hh;
        any av;
        hash<auto> ha;
    }
}

our hash gh;
thread_local hash tlh;

#! members whose types bind reference elements, which a store to them binds after it has released the object's lock
class WscRefHolder {
    public {
        list<reference<softint>> rl;
        hash<string, reference<softint>> rh;
        auto s = "5";
    }
}

#! stores a value weakly in a local that no closure captures, a hash, in a function that tiered execution compiles
int sub weak_store_error(auto v) {
    hash x = {};
    try {
        x := v;
    } catch (hash<ExceptionInfo> ex) {
        return 1;
    }
    return 0;
}

#! returns the number of errors of n calls of weak_store_error() with a typed hash
int sub weak_store_errors(int n) {
    int errs = 0;
    for (int i = 0; i < n; ++i) {
        errs += weak_store_error({"a": 1});
    }
    return errs;
}

sub try_store(string label, code store, code read) {
    try {
        store();
        printf("%s: stored: %y\n", label, read());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
        printf("%s: after: %y\n", label, read());
    }
}

# values whose type is known at run time only
auto src = {"a": 1};
auto lsrc = (1, 2);
auto dsrc = <WscHash>{};
auto empty = {};

{
    list l = (0,);
    try_store("untyped list element", sub () { l[0] := src; }, auto sub () { return l; });
    hash h = {"x": 0};
    try_store("untyped hash key", sub () { h.x := src; }, auto sub () { return h; });
    string k = "x";
    try_store("untyped hash key, dynamic", sub () { h{k} := src; }, auto sub () { return h; });
    any av = 0;
    try_store("any", sub () { av := src; }, auto sub () { return av; });
    hash hv = {};
    try_store("hash", sub () { hv := src; }, auto sub () { return hv; });
    list lv = ();
    try_store("list from list<int>", sub () { lv := lsrc; }, auto sub () { return lv; });
    try_store("hashdecl into hash", sub () { hv := dsrc; }, auto sub () { return hv; });
    try_store("hash<auto> into hash", sub () { hv := empty; }, auto sub () { return hv; });
    WscHolder o();
    try_store("opaque member", sub () { o.hh @= src; }, auto sub () { return o.hh; });
    try_store("opaque any member", sub () { o.av @= src; }, auto sub () { return o.av; });
    try_store("global", sub () { gh := src; }, auto sub () { return gh; });
    try_store("thread_local", sub () { tlh := src; }, auto sub () { return tlh; });
    *list vl;
    try_store("vivified list element", sub () { vl[0] := src; }, auto sub () { return vl; });
    list<softstring> lss = ();
    try_store("list<softstring> from list<int>", sub () { lss := lsrc; }, auto sub () { return lss; });
    hash rh = {};
    reference<hash> r = \rh;
    try_store("through a reference", sub () { r := src; }, auto sub () { return rh; });
}

# locals that no closure captures, which native code stores itself (qore#7572)
{
    hash x = {};
    try {
        x := src;
        printf("plain local hash: stored: %y\n", x);
    } catch (hash<ExceptionInfo> ex) {
        printf("plain local hash: %s; after: %y\n", ex.err, x);
    }
    any a = 0;
    try {
        a := src;
        printf("plain local any: stored: %y\n", a);
    } catch (hash<ExceptionInfo> ex) {
        printf("plain local any: %s; after: %y\n", ex.err, a);
    }
    list<softstring> ls = ();
    try {
        ls := lsrc;
        printf("plain local list<softstring>: stored: %y\n", ls);
    } catch (hash<ExceptionInfo> ex) {
        printf("plain local list<softstring>: %s; after: %y\n", ex.err, ls);
    }
    hash o = {};
    try {
        o @= src;
        printf("plain local opaque: stored: %y\n", o);
    } catch (hash<ExceptionInfo> ex) {
        printf("plain local opaque: %s; after: %y\n", ex.err, o);
    }
    printf("errors in a function called 2000 times: %d\n", weak_store_errors(2000));
}

# a reference: the value is stored in the referenced lvalue, whose type decides
{
    hash<string, int> th = {"a": 1};
    auto ax;
    reference rax = \ax;
    rax := th;
    hash<auto> hx;
    reference rhx = \hx;
    rhx := th;
    th.b = 2;
    printf("reference to auto: %y\n", ax);
    printf("reference to hash<auto>: %y\n", hx);
}

# the lvalues that keep the value: the weak reference designates it and sees its changes
{
    hash<string, int> th = {"a": 1};
    auto av;
    av := th;
    list<auto> la = ();
    la[0] := th;
    hash<auto> ha;
    ha := th;
    hash<string, int> mh;
    mh := th;
    WscHolder o();
    o.ha := th;
    hash ph = {"p": 1};
    hash phv;
    phv := ph;
    th.b = 2;
    ph.q = 2;
    printf("auto: %y\n", av);
    printf("list<auto> element: %y\n", la[0]);
    printf("hash<auto>: %y\n", ha);
    printf("matching type: %y\n", mh);
    printf("hash<auto> member: %y\n", o.ha);
    printf("plain hash into hash: %y\n", phv);
}

# a value with a reference element to a member of the object that it is stored in: the store binds it after it has
# released the object's lock and is made again with the value converted; the conversion to a copy is refused before
# (qore#7391, qore#7572)
{
    WscRefHolder o();
    list<auto> l = (\o.s,);
    auto lx = l;
    try_store("list<reference<softint>> member, reference bound after the lock", sub () { o.rl := lx; },
        auto sub () { return (o.rl, o.s); });
    hash<auto> h = {"a": \o.s};
    auto hx = h;
    try_store("hash<string, reference<softint>> member, reference bound after the lock", sub () { o.rh := hx; },
        auto sub () { return (o.rh, o.s); });
}

# a value that is not a list, which a softlist lvalue wraps in a new list, or no value, which a softlist lvalue (not
# *softlist) converts to a new empty list: the weak or opaque reference would designate the new list, a temporary, so
# the store is refused and the lvalue keeps its value, while a list is stored as before (qore#7619)
class WscWrapObj {
}

class WscListHolder {
    public {
        softlist sl = (0,);
        *softlist nsl = (0,);
        softlist<int> sli = (0,);
        softlist<auto> sla = (0,);
    }
}

our softlist wrap_gsl = (0,);
our *softlist wrap_gnsl = (0,);
our softlist<int> wrap_gsli = (0,);

#! returns what the error of a refused store says about the value: "wrap", "empty", "copy" (qore#7572) or "type"
string sub wrap_kind(string desc) {
    if (desc =~ /wraps the value in a new list/) {
        return "wrap";
    }
    if (desc =~ /converts no value to a new empty list/) {
        return "empty";
    }
    if (desc =~ /converts the value to a copy of its own/) {
        return "copy";
    }
    return "type";
}

sub wrap_case(string label, code store, code read) {
    try {
        store();
        printf("%s: stored: %y\n", label, read());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s (%s); after: %y\n", label, ex.err, wrap_kind(ex.desc), read());
    }
}

string sub wrap_result(*hash<ExceptionInfo> ex, auto after) {
    return ex
        ? sprintf("%s (%s); after: %y", ex.err, wrap_kind(ex.desc), after)
        : sprintf("stored: %y", after);
}

# locals that no closure captures, which native code stores itself
string sub wrap_plain_sl(any v, bool opaque) {
    softlist x = (0,);
    try {
        if (opaque) {
            x @= v;
        } else {
            x := v;
        }
    } catch (hash<ExceptionInfo> ex) {
        return wrap_result(ex, x);
    }
    return wrap_result(NOTHING, x);
}

string sub wrap_plain_nsl(any v, bool opaque) {
    *softlist x = (0,);
    try {
        if (opaque) {
            x @= v;
        } else {
            x := v;
        }
    } catch (hash<ExceptionInfo> ex) {
        return wrap_result(ex, x);
    }
    return wrap_result(NOTHING, x);
}

string sub wrap_plain_sli(any v, bool opaque) {
    softlist<int> x = (0,);
    try {
        if (opaque) {
            x @= v;
        } else {
            x := v;
        }
    } catch (hash<ExceptionInfo> ex) {
        return wrap_result(ex, x);
    }
    return wrap_result(NOTHING, x);
}

string sub wrap_plain_sla(any v, bool opaque) {
    softlist<auto> x = (0,);
    try {
        if (opaque) {
            x @= v;
        } else {
            x := v;
        }
    } catch (hash<ExceptionInfo> ex) {
        return wrap_result(ex, x);
    }
    return wrap_result(NOTHING, x);
}

#! stores a value weakly in a softlist local that no closure captures, in a function that tiered execution compiles
int sub wrap_store_error(any v) {
    softlist x = ();
    try {
        x := v;
    } catch (hash<ExceptionInfo> ex) {
        return 1;
    }
    return 0;
}

#! stores v weakly (:=) and opaquely (@=) in each kind of lvalue of the softlist types
sub wrap_store_all(string src, any v) {
    foreach bool opaque in ((False, True)) {
        string p = src + (opaque ? " @= " : " := ");
        # locals that closures capture
        softlist sl = (0,);
        wrap_case(p + "closure local softlist", sub () { if (opaque) { sl @= v; } else { sl := v; } },
            auto sub () { return sl; });
        *softlist nsl = (0,);
        wrap_case(p + "closure local *softlist", sub () { if (opaque) { nsl @= v; } else { nsl := v; } },
            auto sub () { return nsl; });
        softlist<int> sli = (0,);
        wrap_case(p + "closure local softlist<int>", sub () { if (opaque) { sli @= v; } else { sli := v; } },
            auto sub () { return sli; });
        softlist<auto> sla = (0,);
        wrap_case(p + "closure local softlist<auto>", sub () { if (opaque) { sla @= v; } else { sla := v; } },
            auto sub () { return sla; });
        # locals that no closure captures
        printf("%splain local softlist: %s\n", p, wrap_plain_sl(v, opaque));
        printf("%splain local *softlist: %s\n", p, wrap_plain_nsl(v, opaque));
        printf("%splain local softlist<int>: %s\n", p, wrap_plain_sli(v, opaque));
        printf("%splain local softlist<auto>: %s\n", p, wrap_plain_sla(v, opaque));
        # globals
        wrap_gsl = (0,);
        wrap_case(p + "global softlist", sub () { if (opaque) { wrap_gsl @= v; } else { wrap_gsl := v; } },
            auto sub () { return wrap_gsl; });
        wrap_gnsl = (0,);
        wrap_case(p + "global *softlist", sub () { if (opaque) { wrap_gnsl @= v; } else { wrap_gnsl := v; } },
            auto sub () { return wrap_gnsl; });
        wrap_gsli = (0,);
        wrap_case(p + "global softlist<int>", sub () { if (opaque) { wrap_gsli @= v; } else { wrap_gsli := v; } },
            auto sub () { return wrap_gsli; });
        # object members
        WscListHolder o();
        wrap_case(p + "member softlist", sub () { if (opaque) { o.sl @= v; } else { o.sl := v; } },
            auto sub () { return o.sl; });
        wrap_case(p + "member *softlist", sub () { if (opaque) { o.nsl @= v; } else { o.nsl := v; } },
            auto sub () { return o.nsl; });
        wrap_case(p + "member softlist<int>", sub () { if (opaque) { o.sli @= v; } else { o.sli := v; } },
            auto sub () { return o.sli; });
        # hash keys
        hash<string, softlist> hsl = {"k": (0,)};
        wrap_case(p + "hash key softlist", sub () { if (opaque) { hsl.k @= v; } else { hsl.k := v; } },
            auto sub () { return hsl; });
        hash<string, *softlist> hnsl = {"k": (0,)};
        wrap_case(p + "hash key *softlist", sub () { if (opaque) { hnsl.k @= v; } else { hnsl.k := v; } },
            auto sub () { return hnsl; });
        hash<string, softlist<int>> hsli = {"k": (0,)};
        wrap_case(p + "hash key softlist<int>", sub () { if (opaque) { hsli.k @= v; } else { hsli.k := v; } },
            auto sub () { return hsli; });
        # list elements
        list<softlist> lsl = ((0,),);
        wrap_case(p + "list element softlist", sub () { if (opaque) { lsl[0] @= v; } else { lsl[0] := v; } },
            auto sub () { return lsl; });
        list<*softlist> lnsl = ((0,),);
        wrap_case(p + "list element *softlist", sub () { if (opaque) { lnsl[0] @= v; } else { lnsl[0] := v; } },
            auto sub () { return lnsl; });
        list<softlist<int>> lsli = ((0,),);
        wrap_case(p + "list element softlist<int>", sub () { if (opaque) { lsli[0] @= v; } else { lsli[0] := v; } },
            auto sub () { return lsli; });
    }
}

{
    WscWrapObj obj();
    list<auto> auto_list = (1, 2);
    list<list<auto>> sources = (
        ("object", obj),
        ("int", 1),
        ("string", "s"),
        ("hash", {"a": 1}),
        ("NOTHING", NOTHING),
        ("NULL", NULL),
        ("list", (1, 2)),
        ("list<auto>", auto_list),
    );
    foreach list<auto> s in (sources) {
        wrap_store_all(s[0], s[1]);
    }

    # the error describes the value, the lvalue's type and the fix
    any av = obj;
    softlist dl = (0,);
    try_store("wrap description, object", sub () { dl := av; }, auto sub () { return dl; });
    any ai = 1;
    *softlist dnl = (0,);
    try_store("wrap description, int, opaque", sub () { dnl @= ai; }, auto sub () { return dnl; });
    any ah = {"a": 1};
    softlist<auto> dla = (0,);
    try_store("wrap description, hash", sub () { dla := ah; }, auto sub () { return dla; });
    any an;
    try_store("wrap description, NOTHING", sub () { dl := an; }, auto sub () { return dl; });

    # a weak reference read from a variable as the value assigned: its target is wrapped
    hash ph = {"a": 1};
    any wh;
    wh := ph;
    any wo;
    wo := obj;
    softlist wl = (0,);
    wrap_case("weak reference to a hash := closure local softlist", sub () { wl := wh; }, auto sub () { return wl; });
    wrap_case("weak reference to a hash @= closure local softlist", sub () { wl @= wh; }, auto sub () { return wl; });
    wrap_case("weak reference to an object := closure local softlist", sub () { wl := wo; },
        auto sub () { return wl; });
    wrap_case("weak reference to an object @= closure local softlist", sub () { wl @= wo; },
        auto sub () { return wl; });
    wrap_gsl = (0,);
    wrap_case("weak reference to a hash := global softlist", sub () { wrap_gsl := wh; },
        auto sub () { return wrap_gsl; });
    WscListHolder o();
    wrap_case("weak reference to an object := member softlist", sub () { o.sl := wo; },
        auto sub () { return o.sl; });
    printf("weak reference to a hash := plain local softlist: %s\n", wrap_plain_sl(wh, False));
    printf("weak reference to an object @= plain local softlist: %s\n", wrap_plain_sl(wo, True));
    {
        softlist x = (0,);
        try {
            x := wh;
            printf("weak reference to a hash := plain local softlist, read directly: stored: %y\n", x);
        } catch (hash<ExceptionInfo> ex) {
            printf("weak reference to a hash := plain local softlist, read directly: %s (%s); after: %y\n", ex.err,
                wrap_kind(ex.desc), x);
        }
        try {
            x @= wo;
            printf("weak reference to an object @= plain local softlist, read directly: stored: %y\n", x);
        } catch (hash<ExceptionInfo> ex) {
            printf("weak reference to an object @= plain local softlist, read directly: %s (%s); after: %y\n",
                ex.err, wrap_kind(ex.desc), x);
        }
    }

    int errs = 0;
    for (int i = 0; i < 2000; ++i) {
        errs += wrap_store_error(obj);
    }
    printf("wrap errors in a function called 2000 times: %d\n", errs);
}

# an iterator that a list<T> lvalue would materialize to a new list, which the reference would designate: refused in
# every mode before it is iterated, while an auto lvalue keeps the iterator object (qore#7619); and NULL, which a
# softlist<T> whose element type takes it wraps in a new list of one element (the error says so)
#! returns what the error of a refused store says, or "other" for an error of another kind
string sub iter_kind(string desc) {
    if (desc =~ /builds a new list from the iterator/) {
        return "iterator";
    }
    return wrap_kind(desc);
}

string sub iter_result(*hash<ExceptionInfo> ex, auto after) {
    return ex
        ? sprintf("%s (%s); after: %y", ex.err, iter_kind(ex.desc), after)
        : sprintf("stored: %y", after);
}

# locals that no closure captures, which native code stores itself
string sub iter_plain_list(any v, bool opaque) {
    list<int> x = (0,);
    try {
        if (opaque) {
            x @= v;
        } else {
            x := v;
        }
    } catch (hash<ExceptionInfo> ex) {
        return iter_result(ex, x);
    }
    return iter_result(NOTHING, x);
}

string sub iter_plain_typed_list(ListIterator v, bool opaque) {
    list<int> x = (0,);
    try {
        if (opaque) {
            x @= v;
        } else {
            x := v;
        }
    } catch (hash<ExceptionInfo> ex) {
        return iter_result(ex, x);
    }
    return iter_result(NOTHING, x);
}

string sub iter_plain_softlist(any v, bool opaque) {
    softlist x = (0,);
    try {
        if (opaque) {
            x @= v;
        } else {
            x := v;
        }
    } catch (hash<ExceptionInfo> ex) {
        return iter_result(ex, x);
    }
    return iter_result(NOTHING, x);
}

string sub iter_plain_auto(any v, bool opaque) {
    auto x = (0,);
    try {
        if (opaque) {
            x @= v;
        } else {
            x := v;
        }
    } catch (hash<ExceptionInfo> ex) {
        return iter_result(ex, x);
    }
    return sprintf("stored: %s", x.className());
}

string sub null_plain_sl(any v, bool opaque) {
    softlist<*int> x = (0,);
    try {
        if (opaque) {
            x @= v;
        } else {
            x := v;
        }
    } catch (hash<ExceptionInfo> ex) {
        return iter_result(ex, x);
    }
    return iter_result(NOTHING, x);
}

string sub null_plain_nsl(any v, bool opaque) {
    *softlist<*int> x = (0,);
    try {
        if (opaque) {
            x @= v;
        } else {
            x := v;
        }
    } catch (hash<ExceptionInfo> ex) {
        return iter_result(ex, x);
    }
    return iter_result(NOTHING, x);
}

{
    foreach bool opaque in ((False, True)) {
        string op = opaque ? "@=" : ":=";
        # a function called many times, which tiered execution compiles
        for (int i = 0; i < 3; ++i) {
            printf("iterator %s plain local list<int> (%d): %s\n", op, i,
                iter_plain_list(new ListIterator((1, 2)), opaque));
            printf("iterator %s plain local list<int>, typed (%d): %s\n", op, i,
                iter_plain_typed_list(new ListIterator((1, 2)), opaque));
            printf("iterator %s plain local softlist (%d): %s\n", op, i,
                iter_plain_softlist(new ListIterator((1, 2)), opaque));
            printf("iterator %s plain local auto (%d): %s\n", op, i,
                iter_plain_auto(new ListIterator((1, 2)), opaque));
        }
        ListIterator it((1, 2));
        any ait = it;
        list<int> cl = (0,);
        try {
            if (opaque) {
                cl @= ait;
            } else {
                cl := ait;
            }
            printf("iterator %s closure local list<int>: stored: %y\n", op, cl);
        } catch (hash<ExceptionInfo> ex) {
            printf("iterator %s closure local list<int>: %s (%s); after: %y\n", op, ex.err, iter_kind(ex.desc), cl);
        }
        code c = sub () { cl = (0,); };
        c();
        printf("iterator %s left the iterator at its start: %y\n", op, it.next() ? it.getValue() : NOTHING);
        printf("NULL %s plain local softlist<*int>: %s\n", op, null_plain_sl(NULL, opaque));
        printf("NULL %s plain local *softlist<*int>: %s\n", op, null_plain_nsl(NULL, opaque));
        printf("NOTHING %s plain local softlist<*int>: %s\n", op, null_plain_sl(NOTHING, opaque));
    }
    # the error describes the iterator, the lvalue's type and the fix
    list<int> dl = (0,);
    any dit = new ListIterator((1, 2));
    try_store("iterator description", sub () { dl := dit; }, auto sub () { return dl; });
}
