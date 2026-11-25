%modern
%allow-weak-references

# a program of weak-reference-remove.qtest: remove and delete through a weak reference held by the entry or the key
# that they remove from the container that it designates (qore#7402); the removal frees or moves the storage of the
# lvalue, which is not read afterwards (valgrind reported invalid reads of it), and a removal that ends a cycle
# through an object lets the object be collected; every mode must print the output of the AST interpreter

class C {
    public {
        hash<auto> h;
        list<auto> l;
    }

    private {
        string name;
    }

    constructor(string n) {
        name = n;
    }

    destructor() {
        printf("%s destroyed\n", name);
    }
}

#! the entry that holds the weak reference is removed through it
sub own_entry() {
    list<auto!> l = (NOTHING, 2, 3);
    auto w;
    w := l;
    w[0] := l;
    remove w;
    auto r = remove l[0][0];
    printf("remove own entry: %s %y\n", r.typeCode() == NT_LIST ? "the list" : sprintf("%y", r), l.size());

    list<auto!> l2 = (NOTHING, 2, 3);
    auto w2;
    w2 := l2;
    w2[0] := l2;
    remove w2;
    delete l2[0][0];
    printf("delete own entry: %y\n", l2.size());

    list<auto!> l3 = (1, NOTHING, 3);
    auto w3;
    w3 := l3;
    w3[1] := l3;
    remove w3;
    auto r3 = remove l3[1][0..1];
    printf("remove range over own entry: %d %y\n", r3.size(), l3);

    list<auto!> l4 = (1, NOTHING, 3);
    auto w4;
    w4 := l4;
    w4[1] := l4;
    remove w4;
    auto r4 = remove l4[1][1, 2];
    printf("remove elements with own entry: %d %y\n", r4.size(), l4);
}

#! the key that holds the weak reference is removed through it
sub own_key() {
    hash<auto!> h = {"a": 1, "b": 2};
    auto w;
    w := h;
    w.self := h;
    remove w;
    auto r = remove h.self.self;
    printf("remove own key: %s %y\n", r.typeCode() == NT_HASH ? "the hash" : sprintf("%y", r), keys h);

    hash<auto!> h2 = {"a": 1};
    auto w2;
    w2 := h2;
    w2.self := h2;
    remove w2;
    delete h2.self.self;
    printf("delete own key: %y\n", keys h2);

    hash<auto!> h3 = {"a": 1, "b": 2};
    auto w3;
    w3 := h3;
    w3.self := h3;
    remove w3;
    hash<auto> r3 = remove h3.self{"self", "a"};
    printf("remove keys with own key: %y %y\n", keys r3, keys h3);
}

#! the path crosses the weak step before the entry or the key removed, and slices remove an object, which changes
#! the object count of the container: the IR and JIT removers read the lvalue's type before the removal
sub deeper_and_slices() {
    hash<auto!> h = {"a": 1};
    auto w;
    w := h;
    w.self := h;
    remove w;
    auto r = remove h.self.self.self;
    printf("remove own key, deeper: %s %y\n", r.typeCode() == NT_HASH ? "the hash" : sprintf("%y", r), keys h);

    list<auto!> l = (NOTHING, 2);
    auto wl;
    wl := l;
    wl[0] := l;
    remove wl;
    delete l[0][0][0];
    printf("delete own entry, deeper: %y\n", l.size());

    list<auto!> l1 = (1, NOTHING, 3);
    auto wl1;
    wl1 := l1;
    wl1[1] := l1;
    remove wl1;
    auto r1 = remove l1[1][1][0..1];
    printf("remove range over own entry, deeper: %d %y\n", r1.size(), l1);

    {
        hash<auto!> h2 = {"o": new C("hash slice")};
        auto w2;
        w2 := h2;
        w2.self := h2;
        remove w2;
        hash<auto> r2 = remove h2.self{"self", "o"};
        printf("remove keys with own key and an object: %y\n", keys r2);
    }
    {
        list<auto!> l2 = (new C("list slice"), NOTHING);
        auto w3;
        w3 := l2;
        w3[1] := l2;
        remove w3;
        list<auto> r3 = remove l2[1][0, 1];
        printf("remove elements with own entry and an object: %d\n", r3.size());
    }
}

#! a removal through a weak reference that ends a cycle through an object
sub cycles() {
    {
        C c("hash");
        c.h.o = c;
        auto w;
        w := c.h;
        remove w.o;
        printf("hash: removed\n");
    }
    printf("after hash\n");
    {
        C c("list");
        c.l = (1, c);
        auto w;
        w := c.l;
        remove w[1];
        printf("list: removed\n");
    }
    printf("after list\n");
    {
        C c("range");
        c.l = (1, c, 2);
        auto w;
        w := c.l;
        remove w[1..2];
        printf("range: removed\n");
    }
    printf("after range\n");
}

own_entry();
own_key();
deeper_and_slices();
cycles();
