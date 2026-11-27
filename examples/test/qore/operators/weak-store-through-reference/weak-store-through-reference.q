#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# weak (:=), opaque (@=) and Normal (=) assignments to a local that holds a reference at run time: each assigns the
# value assigned to the referenced lvalue with that lvalue's type, as the AST interpreter does, so that a weak or
# opaque assignment of a container that the referenced lvalue's type converts to a copy is refused with
# RUNTIME-TYPE-ERROR (issue #7572), one into an auto lvalue stores a reference to the value assigned, and a Normal
# assignment through a local of type any keeps the value's type in an auto lvalue (issue #7617)

%modern
%allow-weak-references
%allow-opaque-references

class WstObj {
    public {
        int a = 1;
    }
}

#! prints a value and its type
sub show(string label, auto v) {
    printf("%s: %y %s\n", label, v, v.fullType());
}

#! prints an exception
sub show_error(string label, hash<ExceptionInfo> ex) {
    printf("%s: %s: %s\n", label, ex.err, ex.desc);
}

#! stores weakly through an any parameter that is given a reference
sub param_any_weak(any r, auto v) {
    r := v;
}

#! stores opaquely through an auto parameter that is given a reference
sub param_auto_opaque(auto r, auto v) {
    r @= v;
}

#! stores through an any parameter that is given a reference
sub param_any_normal(any r, auto v) {
    r = v;
}

#! stores through a local of this function that holds a reference, which this function creates
sub function_body(hash<string, int> th, list<int> tl, WstObj obj) {
    auto x;
    any r = \x;
    r := th;
    show("function auto := hash<string, int>", x);
    th.b = 2;
    show("function auto follows the source", x);
    hash hx = {"old": 1};
    any rh = \hx;
    try {
        rh := th;
        show("function hash := hash<string, int>: stored", hx);
    } catch (hash<ExceptionInfo> ex) {
        show_error("function hash := hash<string, int>", ex);
        show("function hash after the error", hx);
    }
    list lx = ("old",);
    reference rl = \lx;
    try {
        rl @= tl;
        show("function list @= list<int>: stored", lx);
    } catch (hash<ExceptionInfo> ex) {
        show_error("function list @= list<int>", ex);
        show("function list after the error", lx);
    }
    auto ox;
    any ro = \ox;
    auto v = (ro := obj);
    printf("function auto := object: value: %y, target: %y\n", v.a, ox.a);
    obj.a = 2;
    printf("function auto := object: target follows: %y\n", ox.a);
    auto nx;
    any rn = \nx;
    rn = th;
    show("function auto = hash<string, int> through any", nx);
    hash nh;
    any rnh = \nh;
    rnh = th;
    show("function hash = hash<string, int> through any", nh);
}

#! stores through a block-scoped local that holds a reference
sub block_scoped(auto th) {
    auto x;
    hash hx = {"old": 1};
    {
        any r = \x;
        auto v = (r @= th);
        show("block auto @= hash<string, int>: value", v);
        show("block auto @= hash<string, int>: target", x);
    }
    {
        reference<hash> r = \hx;
        try {
            r := th;
            show("block reference<hash> := hash<string, int>: stored", hx);
        } catch (hash<ExceptionInfo> ex) {
            show_error("block reference<hash> := hash<string, int>", ex);
            show("block hash after the error", hx);
        }
    }
    {
        auto y;
        any r = \y;
        r = th;
        show("block auto = hash<string, int> through any", y);
    }
}

#! stores through a local whose reference arrives as a list element (a reference slot local)
sub reference_slot(list<auto> l, hash<string, int> th) {
    any r = l[0];
    r = th;
}

#! stores through a local that a closure captures
sub closure_captured(hash<string, int> th) {
    auto x;
    any r = \x;
    code c = sub () { r := th; };
    c();
    show("closure auto := hash<string, int>", x);
    hash hx = {"old": 1};
    any rh = \hx;
    code ch = sub () { rh := th; };
    try {
        ch();
        show("closure hash := hash<string, int>: stored", hx);
    } catch (hash<ExceptionInfo> ex) {
        show_error("closure hash := hash<string, int>", ex);
        show("closure hash after the error", hx);
    }
}

#! stores a local's own value or through a reference that it holds, as \a i is 0 or not
int sub rebound(int i, hash<string, int> th) {
    auto x;
    any r = i ? \x : 0;
    try {
        r := th;
    } catch (hash<ExceptionInfo> ex) {
        return 1;
    }
    return x.a == 1 ? 0 : 2;
}

#! called a few thousand times, so that tiered execution compiles it: stores weakly through a reference
int sub hot_through(hash<string, int> th) {
    auto x;
    any r = \x;
    int rv = (r := th).a;
    return rv + x.a;
}

#! called a few thousand times, so that tiered execution compiles it: a store refused through a reference
int sub hot_refused(hash<string, int> th) {
    hash hx = {};
    any r = \hx;
    try {
        r := th;
    } catch (hash<ExceptionInfo> ex) {
        return hx ? 2 : 1;
    }
    return 0;
}

#! called a few thousand times, so that tiered execution compiles it: a Normal store through any
string sub hot_normal(hash<string, int> th) {
    auto x;
    any r = \x;
    r = th;
    return x.fullType();
}

# referenced lvalues that are not locals: globals, members and container elements (issue #7617)
our hash wst_g_hash = {"old": 1};
our auto wst_g_auto;
our hash<auto!> wst_g_hauto = {};
our list<auto!> wst_g_lauto = ();

class WstHolder {
    public {
        hash hm = {"old": 1};
        auto am;
        hash<auto!> hauto = {};
    }
}

#! runs a store that may be refused and prints the lvalue afterwards
sub try_store(string label, code store, code read) {
    try {
        store();
        show(label + ": stored", read());
    } catch (hash<ExceptionInfo> ex) {
        show_error(label, ex);
        show(label + " after the error", read());
    }
}

#! stores weakly through a reference parameter
sub param_ref_weak(reference r, auto v) {
    r := v;
}

#! stores opaquely through a reference parameter
sub param_ref_opaque(reference r, auto v) {
    r @= v;
}

#! stores weakly through a reference<hash> parameter
sub param_typed_ref_weak(reference<hash> r, auto v) {
    r := v;
}

#! stores weakly through a reference parameter that a closure captures
sub param_ref_closure(reference r, auto v) {
    code c = sub () { r := v; };
    c();
}

#! stores through locals that hold references to globals, members and elements, and through reference parameters
sub other_lvalues(hash<string, int> th, list<int> tl) {
    # the sources are locals of this function, so that a weak reference to them designates the value itself
    hash<string, int> src = th + {};
    list<int> lsrc = tl + ();
    hash plain = {"p": 1};
    list plain_list = (1, 2);

    # globals
    reference rg = \wst_g_hash;
    try_store("global hash :=", sub () { rg := src; }, auto sub () { return wst_g_hash; });
    any rga = \wst_g_auto;
    rga := src;
    src.b = 2;
    show("global auto := hash<string, int>, after a change of the source", wst_g_auto);
    reference rgh = \wst_g_hauto;
    try_store("global hash<auto!> := hash", sub () { rgh := plain; }, auto sub () { return wst_g_hauto; });
    try_store("global hash<auto!> := hash<string, int>", sub () { rgh := src; },
        auto sub () { return wst_g_hauto; });
    any rgl = \wst_g_lauto;
    try_store("global list<auto!> := list", sub () { rgl := plain_list; }, auto sub () { return wst_g_lauto; });
    try_store("global list<auto!> @= list<int>", sub () { rgl @= lsrc; }, auto sub () { return wst_g_lauto; });

    # members
    WstHolder hld();
    reference rm = \hld.hm;
    try_store("member hash :=", sub () { rm := src; }, auto sub () { return hld.hm; });
    any rma = \hld.am;
    rma @= src;
    show("member auto @= hash<string, int>", hld.am);
    reference rmh = \hld.hauto;
    try_store("member hash<auto!> := hash", sub () { rmh := plain; }, auto sub () { return hld.hauto; });
    try_store("member hash<auto!> := hash<string, int>", sub () { rmh := src; },
        auto sub () { return hld.hauto; });

    # container elements
    hash h = {"k": 1};
    reference rk = \h.k;
    try_store("untyped hash key :=", sub () { rk := src; }, auto sub () { return h.k; });
    hash<string, auto> ha = {"k": 1, "s": "x"};
    any rka = \ha.k;
    rka := src;
    show("hash<string, auto> key := hash<string, int>", ha.k);
    list l = (1,);
    reference re = \l[0];
    try_store("untyped list element :=", sub () { re := src; }, auto sub () { return l[0]; });
    list<auto> la = (1, "x");
    any rea = \la[0];
    rea := lsrc;
    show("list<auto> element := list<int>", la[0]);

    # a plain container keeps the source in a hash<auto!> or list<auto!> lvalue (issue #7572)
    hash<auto!> hau = {};
    any rh = \hau;
    try_store("local hash<auto!> := hash", sub () { rh := plain; }, auto sub () { return hau; });
    plain.q = 2;
    show("local hash<auto!> after a change of the source", hau);
    try_store("local hash<auto!> := hash<string, int>", sub () { rh := src; }, auto sub () { return hau; });
    list<auto!> lau = ();
    reference rl = \lau;
    try_store("local list<auto!> @= list", sub () { rl @= plain_list; }, auto sub () { return lau; });
    try_store("local list<auto!> := list<int>", sub () { rl := lsrc; }, auto sub () { return lau; });

    # reference parameters
    hash hx = {"old": 1};
    try_store("reference parameter :=", sub () { param_ref_weak(\hx, src); }, auto sub () { return hx; });
    try_store("reference parameter @=", sub () { param_ref_opaque(\hx, src); }, auto sub () { return hx; });
    try_store("reference<hash> parameter :=", sub () { param_typed_ref_weak(\hx, src); },
        auto sub () { return hx; });
    try_store("reference parameter to a global :=", sub () { param_ref_weak(\wst_g_hash, src); },
        auto sub () { return wst_g_hash; });
    try_store("reference parameter to a member @=", sub () { param_ref_opaque(\hld.hm, src); },
        auto sub () { return hld.hm; });
    try_store("reference parameter in a closure :=", sub () { param_ref_closure(\hx, src); },
        auto sub () { return hx; });
}

hash<string, int> th = {"a": 1};
list<int> tl = (1, 2);
WstObj obj();

# top-level locals: a reference local to a hash
{
    hash plain = {"old": 1};
    reference rp = \plain;
    try {
        rp := th;
        show("top-level hash := hash<string, int>: stored", plain);
    } catch (hash<ExceptionInfo> ex) {
        show_error("top-level hash := hash<string, int>", ex);
        show("top-level hash after the error", plain);
    }
    try {
        rp @= th;
        show("top-level hash @= hash<string, int>: stored", plain);
    } catch (hash<ExceptionInfo> ex) {
        show_error("top-level hash @= hash<string, int>", ex);
        show("top-level hash after the error", plain);
    }
    try {
        rp := obj;
        show("top-level hash := object: stored", plain);
    } catch (hash<ExceptionInfo> ex) {
        show_error("top-level hash := object", ex);
        show("top-level hash after the error", plain);
    }
    rp = th;
    show("top-level hash = hash<string, int> through reference", plain);
}

# top-level locals: an any local to an auto
{
    hash<string, int> src = {"a": 1};
    auto x;
    any r = \x;
    auto v = (r := src);
    show("top-level auto := hash<string, int>: value", v);
    show("top-level auto := hash<string, int>: target", x);
    # the value of the assignment shares the source's container, which the change below would copy
    remove v;
    src.b = 2;
    show("top-level auto follows the source", x);
    list<int> lsrc = (1, 2);
    auto lx;
    auto rl = \lx;
    auto lv = (rl @= lsrc);
    show("top-level auto @= list<int> through auto: value", lv);
    remove lv;
    lsrc += 3;
    show("top-level auto @= list<int> after a change of the source", lx);
    # an object
    WstObj o();
    auto ox;
    any ro = \ox;
    ro := o;
    o.a = 3;
    printf("top-level auto := object: target follows: %y\n", ox.a);
}

# top-level locals: typed targets
{
    hash<string, int> tx = {"old": 1};
    any r = \tx;
    r := th;
    show("top-level hash<string, int> := hash<string, int>", tx);
    any ax = 1;
    reference ra = \ax;
    try {
        ra := th;
        show("top-level any := hash<string, int>: stored", ax);
    } catch (hash<ExceptionInfo> ex) {
        show_error("top-level any := hash<string, int>", ex);
        show("top-level any after the error", ax);
    }
    ra := obj;
    printf("top-level any := object: %y\n", ax.a);
    auto ox;
    reference<auto> ro = \ox;
    ro := obj;
    printf("top-level reference<auto> := object: %y\n", ox.a);
}

# top-level locals: Normal stores through an any or auto local
{
    auto x;
    any r = \x;
    r = th;
    show("top-level auto = hash<string, int> through any", x);
    hash hx;
    any rh = \hx;
    rh = th;
    show("top-level hash = hash<string, int> through any", hx);
    auto lx;
    any rl = \lx;
    rl = tl;
    show("top-level auto = list<int> through any", lx);
    list llx;
    auto rll = \llx;
    rll = tl;
    show("top-level list = list<int> through auto", llx);
}

# a top-level hot loop
{
    auto x;
    hash hx = {"old": 1};
    any r = \x;
    any rh = \hx;
    int errs = 0;
    for (int i = 0; i < 50; ++i) {
        hash<string, int> h = {"a": i};
        r := h;
        if (x.a != i) {
            printf("loop: %d: %y\n", i, x);
        }
        try {
            rh := h;
        } catch (hash<ExceptionInfo> ex) {
            ++errs;
        }
    }
    printf("loop: refused: %d, target: %y\n", errs, hx);
}

# parameters given a reference
{
    auto x;
    param_any_weak(\x, th);
    show("any parameter := hash<string, int>", x);
    hash hx = {"old": 1};
    try {
        param_any_weak(\hx, th);
        show("any parameter into hash: stored", hx);
    } catch (hash<ExceptionInfo> ex) {
        show_error("any parameter into hash", ex);
        show("any parameter into hash after the error", hx);
    }
    auto y;
    param_auto_opaque(\y, tl);
    show("auto parameter @= list<int>", y);
    auto z;
    param_any_normal(\z, th);
    show("any parameter = hash<string, int>", z);
    hash hz;
    param_any_normal(\hz, th);
    show("any parameter = hash<string, int> into hash", hz);
}

function_body(th, tl, obj);
block_scoped(th);

# a reference slot local
{
    auto x;
    reference_slot((\x,), th);
    show("reference slot auto = hash<string, int>", x);
    hash hx;
    reference_slot((\hx,), th);
    show("reference slot hash = hash<string, int>", hx);
}

closure_captured(th);
other_lvalues(th, tl);

# an unbound reference local
{
    reference ru;
    try {
        ru := th;
        show("unbound reference := hash<string, int>: stored", ru);
    } catch (hash<ExceptionInfo> ex) {
        show_error("unbound reference := hash<string, int>", ex);
    }
}

# a local that holds no reference keeps the refusal of issue #7572
{
    any never = 1;
    try {
        never := th;
        show("any := hash<string, int>: stored", never);
    } catch (hash<ExceptionInfo> ex) {
        show_error("any := hash<string, int>", ex);
        show("any after the error", never);
    }
    printf("rebound: no reference: %d, reference: %d\n", rebound(0, th), rebound(1, th));
}

# functions that tiered execution compiles
{
    int through_sum = 0;
    int refused_sum = 0;
    hash<string, int> types;
    for (int i = 0; i < 3000; ++i) {
        through_sum += hot_through(th);
        refused_sum += hot_refused(th);
        ++types{hot_normal(th)};
    }
    printf("hot: through: %d, refused: %d, types: %y\n", through_sum, refused_sum, types);
}
