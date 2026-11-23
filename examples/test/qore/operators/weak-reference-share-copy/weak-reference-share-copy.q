#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# A list or hash that a weak reference designates is a value: a holder that takes it (an assignment, a parameter, a
# list element, a hash key, an object member, a foreach variable) takes a copy of its own, so that a change made
# through the weak reference reaches only the holder that the weak reference was made from; only a local variable that
# takes one that nothing else holds keeps it, and the weak reference follows it (qore#7342)

%modern
%allow-weak-references

class O {
    public {
        auto m;
    }
}

sub take_param(list<auto> p, reference<list<auto>> rw) {
    rw = p;
}

list<auto> sub ret(list<auto> l) {
    return l;
}

list<auto> sub observe(list<auto> p, code write) {
    write();
    return p;
}

int sub observe_int(list<auto> p, code write) {
    write();
    return p[0];
}

# a parameter holds a copy of its own while the caller's list is changed through a weak reference
sub parameter_observed() {
    list<auto> src = (1, 2);
    auto wl;
    wl := src;
    list<auto> r = observe(src, sub () { wl[0] = 9; });
    wl[0] = 1;
    int i = observe_int(src, sub () { wl[0] = 7; });
    printf("parameter observed: src: %y, r: %y, i: %y\n", src, r, i);
}

# the issue's case: a local assigned the list
sub local_assignment() {
    list<auto> src = (1, 2);
    auto wl;
    wl := src;
    list<auto> b = src;
    wl[0] = 9;
    printf("local assignment: src: %y, b: %y\n", src, b);
}

# the documented limitation (David, qore#7342): a list that only holds a weakly referenced list is shared as any
# other value, and so is the list in it, so that a change through the weak reference reaches b[0] too
sub nested_target() {
    list<auto> src = ((1, 2), 3);
    auto wl;
    wl := src[0];
    list<auto> b = src;
    wl[0] = 9;
    printf("nested: src: %y, b: %y\n", src, b);
}

sub parameter() {
    list<auto> src = (1, 2);
    auto wl;
    wl := src;
    list<auto> p;
    take_param(src, \p);
    wl[0] = 9;
    printf("parameter: src: %y, p: %y\n", src, p);
}

sub list_element() {
    list<auto> src = (1, 2);
    auto wl;
    wl := src;
    list<auto> l = (src,);
    l += (src,);
    wl[0] = 9;
    printf("list element: src: %y, l: %y\n", src, l);
}

sub hash_key() {
    hash<auto> src = {"a": 1};
    auto wh;
    wh := src;
    hash<auto> h = {"k": src};
    h.k2 = src;
    wh.a = 9;
    printf("hash key: src: %y, h: %y\n", src, h);
}

sub object_member() {
    hash<auto> src = {"a": 1};
    auto wh;
    wh := src;
    O o();
    o.m = src;
    wh.a = 9;
    printf("object member: src: %y, o.m: %y\n", src, o.m);
}

sub return_value() {
    list<auto> src = (1, 2);
    auto wl;
    wl := src;
    list<auto> r = ret(src);
    wl[0] = 9;
    printf("return value: src: %y, r: %y\n", src, r);
}

sub foreach_variable() {
    list<auto> src = ((1, 2),);
    auto wl;
    wl := src[0];
    list<auto> seen;
    foreach list<auto> e in (src) {
        wl[0] = 9;
        seen = e;
    }
    printf("foreach: src: %y, seen: %y\n", src, seen);
}

# a hash member path: the hash that holds the weakly referenced one is shared, and so is the one in it (the documented
# limitation), so that the change reaches the hash assigned too
sub member_path() {
    O o();
    o.m = {"x": {"y": 1}};
    auto w;
    w := o.m.x;
    hash<auto> copy = o.m;
    w.y = 9;
    printf("member path: o.m: %y, copy: %y\n", o.m, copy);
}

hash<auto> sub make_weak_target(reference<auto> w) {
    hash<auto> h = {"a": 1};
    w := h;
    return h;
}

# a container that nothing else holds is moved into the local variable that takes it: it keeps its identity, and the
# weak reference follows it; the list that extract returns holds a copy of the element that it removes, as a list
# element always takes a copy (see share_points_of_moves()), so the weak reference to the element designates the
# element removed, which nothing holds then
sub moves() {
    hash<auto> h = {"a": 1};
    auto w;
    w := h;
    hash<auto> r = remove h;
    r.b = 2;
    printf("move remove: w: %y\n", w);

    list<auto> l = ({"x": 1}, 2);
    auto wl;
    wl := l[0];
    hash<auto> s = shift l;
    s.y = 2;
    printf("move shift: wl: %y\n", wl);

    list<auto> l2 = (1, {"x": 1});
    auto wl2;
    wl2 := l2[1];
    hash<auto> p = pop l2;
    p.y = 2;
    printf("move pop: wl2: %y\n", wl2);

    list<auto> l3 = ((1, 2), 3);
    auto wl3;
    wl3 := l3[0];
    list<auto> e = extract l3, 0, 1;
    e[0][0] = 9;
    printf("move extract: wl3: %y\n", wl3);

    auto wf;
    hash<auto> made = make_weak_target(\wf);
    made.b = 2;
    printf("move call result: wf: %y\n", wf);
}

# a copy-on-write copy of a container that holds a weakly designated one shares it, as any copy of a container does
# (4b): the weak reference keeps designating the container that both holders hold
sub copy_on_write() {
    list<auto> src = ((1, 2), 3);
    auto wl;
    wl := src[0];
    list<auto> b = src;
    src[1] = 4;
    wl[0] = 9;
    printf("copy on write: src: %y, b: %y\n", src, b);
    hash<auto> hsrc = {"x": {"y": 1}};
    auto wh;
    wh := hsrc.x;
    hash<auto> hb = hsrc;
    hsrc.z = {"q": 2};
    wh.y = 9;
    printf("copy on write hash: hsrc: %y, hb: %y\n", hsrc, hb);
}

our auto gw;
our hash<auto> gstore;

hash<auto> sub mk() {
    hash<auto> h = {"a": 1};
    gw := h;
    return h;
}

our auto gwl;

list<auto> sub mkl() {
    list<auto> l = (1,);
    gwl := l;
    return l;
}

auto sub pass_through(auto v) {
    return v;
}

class PassThrough {
    auto get(auto v) {
        return v;
    }

    static auto getStatic(auto v) {
        return v;
    }
}

hash<auto> sub param_observe(hash<auto> p) {
    p.b = 2;
    return p;
}

# a container that nothing else holds keeps its identity only when a local variable takes it; any other share point
# copies a weakly designated container, also when nothing else holds it (David 2c, qore#7342): the weak reference
# then designates the temporary, which the statement releases
sub share_points_of_moves() {
    hash<auto> local = mk();
    local.b = 2;
    printf("move into a local: %y\n", gw);

    # += on a local without a value assigns the value added, which a local takes as from an assignment
    auto ax;
    ax += mk();
    ax.b = 2;
    printf("move by += into an empty local: %y\n", gw);

    auto al;
    al += mkl();
    al[0] = 9;
    printf("move of a list by += into an empty local: %y\n", gwl);

    # the same in a try block, where the operation can raise an exception that the block handles
    try {
        auto tx;
        tx += mk();
        tx.b = 2;
        printf("move by += into an empty local in a try block: %y\n", gw);
    } catch (hash<ExceptionInfo> ex) {
        printf("move by += in a try block: %s\n", ex.err);
    }

    # += on NULL assigns the value added as well
    auto nx = NULL;
    nx += mk();
    nx.b = 2;
    printf("move by += into a local holding NULL: %y\n", gw);

    # a variable declared with a type is converted to it by +=, which copies the container
    *hash<auto> tyx;
    tyx += mk();
    tyx.b = 2;
    printf("copy by += into an empty typed local: %y\n", gw);

    # the temporary of each iteration is moved into the variable of that iteration
    list<auto> ws = ();
    for (int i = 0; i < 2; ++i) {
        auto lx;
        lx += mk();
        lx.i = i;
        ws += gw;
    }
    printf("move by += in a loop: %y\n", ws);

    list<auto> l = (mk(),);
    l[0].b = 2;
    printf("move into a list element: %y\n", gw);

    hash<auto> hl = {"k": mk()};
    hl.k.b = 2;
    printf("move into a hash key: %y\n", gw);

    gstore = mk();
    gstore.b = 2;
    printf("move into a global: %y\n", gw);

    hash<auto> cv;
    code c = sub () { cv = mk(); cv.b = 2; };
    c();
    printf("move into a closure variable: %y\n", gw);

    param_observe(mk());
    printf("move into a parameter: %y\n", gw);

    # the parameter takes a copy also when the function only returns it
    auto px = pass_through(mk());
    px.b = 2;
    printf("move into an untyped parameter that the function returns: %y\n", gw);

    PassThrough pt();
    auto mx = pt.get(mk());
    mx.b = 2;
    printf("move into an untyped parameter that a method returns: %y\n", gw);

    auto sx = PassThrough::getStatic(mk());
    sx.b = 2;
    printf("move into an untyped parameter that a static method returns: %y\n", gw);
}

#! runs a case by its name: each case is called by name, so that JIT and tiered execution run it as native code
#! (a call through a call reference runs on the IR tier, and is compiled only when it returns)
sub run_case(string name) {
    switch (name) {
        case "local_assignment": local_assignment(); break;
        case "nested_target": nested_target(); break;
        case "parameter": parameter(); break;
        case "parameter_observed": parameter_observed(); break;
        case "list_element": list_element(); break;
        case "hash_key": hash_key(); break;
        case "object_member": object_member(); break;
        case "return_value": return_value(); break;
        case "foreach_variable": foreach_variable(); break;
        case "member_path": member_path(); break;
        case "moves": moves(); break;
        case "copy_on_write": copy_on_write(); break;
        case "share_points_of_moves": share_points_of_moves(); break;
    }
}

const CaseNames = (
    "local_assignment",
    "nested_target",
    "parameter",
    "parameter_observed",
    "list_element",
    "hash_key",
    "object_member",
    "return_value",
    "foreach_variable",
    "member_path",
    "moves",
    "copy_on_write",
    "share_points_of_moves",
);

# a case named on the command line runs alone
*string only = ARGV[0];
foreach string n in (CaseNames) {
    if (!only || only == n) {
        try {
            run_case(n);
        } catch (hash<ExceptionInfo> ex) {
            printf("%s: %s: %s\n", n, ex.err, ex.desc);
        }
    }
}
printf("done\n");
