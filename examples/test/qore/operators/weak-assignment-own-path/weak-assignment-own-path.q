%modern
%allow-weak-references

# the program of weak-assignment-own-path.qtest: a weak assignment of a container that its target's path passes
# through (h.w := h) designates the container that the lvalue holds, and copies nothing (qore#7405); every mode must
# print the output of the AST interpreter

#! returns a hash h with h.a.b = {"k": 1}, whose containers take any value
hash<auto!> sub mk() {
    hash<auto!> h = {};
    hash<auto!> a = {};
    hash<auto!> b = {"k": 1};
    a.b = b;
    h.a = a;
    return h;
}

#! runs a change, printing its exception
sub run(string label, code c) {
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

#! the container assigned is the root, an inner container, or the container that holds the inner one
sub own_container() {
    hash<auto!> h = {};
    h.w := h;
    run("root", sub () { h.w.k = 1; });
    printf("root: %y\n", keys h);

    hash<auto!> h2 = mk();
    h2.a.w := h2.a;
    run("inner", sub () { h2.a.w.k = 1; });
    printf("inner: %y\n", keys h2.a);

    hash<auto!> h3 = mk();
    h3.a.w := h3;
    run("holder", sub () { h3.a.w.a.k = 1; });
    printf("holder: %y\n", keys h3.a);

    list<auto!> l = (1,);
    l[1] := l;
    run("list", sub () { l[1][2] = 3; });
    printf("list: %d %y\n", l.size(), l[2]);
}

#! a weak reference into the containers below the one assigned designates them still
sub other_weak_references() {
    hash<auto!> h = mk();
    h.a.x := h.a.b;
    h.a.b.w := h;
    printf("x after the assignment: %y\n", h.a.x.k);
    h.a.b.k2 = 2;
    printf("x after a store: %y\n", keys h.a.x);
    # the path of qore#7405: up through w, then down through x
    run("up and down", sub () { h.a.b.w.a.x.o = 1; });
    printf("up and down: %y\n", keys h.a.b);
}

#! the source is shared with another owner: the lvalue's own copy is designated, the other owner's is unchanged
sub shared_source() {
    hash<auto!> h = mk();
    hash<auto!> other = h;
    h.w := h;
    run("shared", sub () { h.w.k = 1; });
    printf("shared: %y other: %y\n", keys h, keys other);
}

#! the source is a local of a function, untyped or typed, or a member of an object
sub local_untyped() {
    auto h = {};
    h.w := h;
    run("local untyped", sub () { h.w.k = 1; });
    printf("local untyped: %y\n", keys h);
}

sub local_typed() {
    hash<auto!> h = mk();
    h.a.x := h.a.b;
    h.a.b.w := h;
    h.a.b.k2 = 2;
    printf("local typed: %y %y\n", keys h.a.x, keys h.a.b.w);
}

class C {
    public {
        hash<auto!> h = {};
    }

    change() {
        h.w := h;
        run("member", sub () { h.w.k = 1; });
        printf("member: %y\n", keys h);
    }
}

#! locals that no closure captures, which keep their storage in the IR and JIT code
sub uncaptured() {
    auto h = {};
    h.w := h;
    h.w.k = 1;
    printf("uncaptured untyped: %y\n", keys h);

    hash<auto!> h2 = mk();
    h2.a.x := h2.a.b;
    h2.a.b.w := h2;
    h2.a.b.k2 = 2;
    printf("uncaptured cascade: %y\n", keys h2.a.x);
}

#! the source is a local that holds a reference: the lvalue that it designates is the source
sub reference_source() {
    hash<auto!> h = {"a": 1};
    auto r = \h;
    hash<auto!> t = {};
    t.w := r;
    t.w.b = 2;
    printf("reference source: %y\n", keys h);
    # the lvalue holds a weak reference to the hash, not the reference: once the variable holds another hash, the
    # weak reference designates the freed original
    h = {"c": 3};
    printf("reference source after the variable changed: %y\n", t.w);

    # a reference to a key that does not exist is no lvalue: the key is not created
    hash<auto!> h2 = {};
    auto r2 = \h2.x;
    hash<auto!> t2 = {};
    t2.w := r2;
    printf("reference to a missing key: %y %y\n", keys h2, t2.w);
}

#! the source holds a weak reference to an object: the object is assigned, which an object lvalue accepts
sub weak_object_source() {
    C c();
    auto a;
    a := c;
    object o;
    o := a;
    printf("weak object source: %y\n", o instanceof C);
}

#! the value of the operator is the value assigned, which a variable that is given it holds as a value of its own
sub operator_value() {
    list<auto> src = (1, 2);
    auto wl;
    auto v = (wl := src);
    v[0] = 9;
    printf("value list: src %y v %y\n", src, v);

    hash<auto> hs = {"a": 1};
    auto wh;
    auto hv = (wh := hs);
    hv.a = 9;
    printf("value hash: src %y v %y\n", hs, hv);
}

our hash<auto!> g = {};

#! the source is a global variable or a variable that a closure captures, on the path of the lvalue
sub global_and_closure() {
    g.w := g;
    g.w.k = 1;
    printf("global: %y\n", keys g);

    hash<auto!> hc = {};
    code f = sub () {
        hc.w := hc;
        hc.w.k = 1;
    };
    f();
    printf("closure: %y\n", keys hc);
}

own_container();
other_weak_references();
shared_source();
local_untyped();
local_typed();
(new C()).change();
uncaptured();
reference_source();
weak_object_source();
global_and_closure();
operator_value();
