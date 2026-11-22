%modern
%allow-weak-references
# the program of weak-reference-unique-source.qtest for lvalue paths: a weak reference to an object member, a member
# of the object, a hash key or a list element designates the container of the path, made unique first if it is shared
# with a literal of the program or another owner, as for a variable (qore#7242); every mode must print the output of
# the AST interpreter

class Holder {
    public {
        list<auto> l;
        hash<auto> h;
        static hash<auto> sh;
    }

    #! a member of the object
    selfMember() {
        l = lit();
        list<auto> w;
        w := l;
        w[0].x = 3;
        printf("self member: l %y literal %y\n", l, lit());
    }
}

#! returns a literal of the program (the AST interpreter returns the literal itself)
list<auto> sub lit() {
    return ({"x": 0}, {"x": 1});
}

#! returns a list that the caller owns
list<auto> sub mk() {
    list<auto> r = ();
    r += {"x": 0};
    r += {"x": 1};
    return r;
}

#! a literal held by the path: a write through the weak reference does not change the literal
sub literals() {
    Holder c();
    c.l = lit();
    list<auto> w;
    w := c.l;
    w[0].x = 7;
    printf("member: c.l %y w %y literal %y\n", c.l, w, lit());

    hash<auto> h = {"k": lit()};
    list<auto> w2;
    w2 := h.k;
    w2[0].x = 8;
    printf("hash key: h.k %y literal %y\n", h.k, lit());

    list<auto!> outer = (lit(),);
    list<auto> w3;
    w3 := outer[0];
    w3[1].x = 9;
    printf("list element: outer[0] %y literal %y\n", outer[0], lit());

    hash<string, hash<auto>> nested = {"a": {"b": lit()}};
    list<auto> w4;
    w4 := nested.a.b;
    w4[0].x = 10;
    printf("nested path: %y literal %y\n", nested.a.b, lit());

    Holder::sh = {"k": lit()};
    list<auto> w5;
    w5 := Holder::sh.k;
    w5[0].x = 11;
    printf("static path: %y literal %y\n", Holder::sh.k, lit());

    c.selfMember();
}

#! a container that the path shares with another variable: the path gets its own, as a variable does
sub shared() {
    list<auto> a = mk();
    hash<auto> h = {"k": a};
    list<auto> w;
    w := h.k;
    w[0].x = 7;
    printf("shared hash key: a %y h.k %y w %y\n", a, h.k, w);

    list<auto> b = mk();
    Holder c();
    c.l = b;
    list<auto> w2;
    w2 := c.l;
    w2[1].x = 5;
    printf("shared member: b %y c.l %y\n", b, c.l);

    # a variable source, for comparison
    list<auto> v = mk();
    list<auto> lv = v;
    list<auto> w3;
    w3 := lv;
    w3[0].x = 6;
    printf("shared variable: v %y lv %y\n", v, lv);
}

#! a container that only the path holds: the weak reference designates it, and later writes of the owner are seen
sub unshared() {
    hash<auto> h = {"k": mk()};
    list<auto> w;
    w := h.k;
    h.k[0].x = 4;
    printf("unshared: w %y\n", w);
    remove h.k;
    printf("after remove: w %y\n", w);
}

#! sources that hold no container, or no value: nothing is created or changed
sub edges() {
    hash<auto> h = {};
    auto w;
    w := h.k;
    printf("missing key: h %y w %y\n", h, w);
    list<auto!> l = (1,);
    auto w2;
    w2 := l[5];
    printf("beyond the end: l %y w2 %y\n", l, w2);
    hash<auto> h2 = {"k": "str"};
    auto w3;
    w3 := h2.k;
    printf("scalar: %y %y\n", h2, w3);
}

#! a class whose undeclared and private members are read through memberGate()
class MemberGateHolder {
    private {
        list<auto> p = (1,);
    }

    auto memberGate(string m) {
        printf("memberGate(%y)\n", m);
        return (9,);
    }
}

int calls;
string sub key(string k) {
    ++calls;
    return k;
}

#! paths that are not made unique, read as before: slices, and members read through memberGate(); the keys and
#! indexes of a path are evaluated once
sub not_paths() {
    hash<auto> h = {"a": (1,), "b": (2,)};
    auto w;
    auto v = (w := h.("a", "b"));
    printf("slice: %y\n", v);
    list<string> ks = ("a", "b");
    v = (w := h{ks});
    printf("runtime slice: %y\n", v);
    auto g = new MemberGateHolder();
    v = (w := g.zz);
    printf("undeclared member: %y\n", v);
    v = (w := g.p);
    printf("private member: %y\n", v);
    hash<auto> h2 = {"a": {"b": mk()}};
    calls = 0;
    list<auto> w2;
    w2 := h2{key("a")}{key("b")};
    printf("key calls: %d\n", calls);
    list<auto!> l = (new Mutex(), (1,));
    int i = 0;
    auto w3;
    w3 := l[i++];
    printf("index: i %d %s\n", i, w3.className());
}

literals();
shared();
unshared();
edges();
not_paths();
