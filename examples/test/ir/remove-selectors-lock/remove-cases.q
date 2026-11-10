%modern
# the cases of remove-selectors-lock.qtest: remove and delete with selectors that read or write the lvalue that they
# remove from, which the AST interpreter evaluated under the lvalue's lock (qore#5696)

# Copyright (C) 2026 Qore Technologies, s.r.o.

our list<auto> g;
our hash<auto> gh;
*int kn;

class Holder {
    public {
        list<auto> l = (1, 2, 3, 4);
        static list<auto> sl = (1, 2, 3, 4);
    }
}

Holder o = new Holder();

class Obj {
    public {
        string name;
    }

    constructor(string n) {
        name = n;
    }

    destructor() {
        print("  destroy " + name + "\n");
    }
}

our list<auto> go;

#! records an evaluation
int sub f(string tag, int rv) {
    print("  sel " + tag + "\n");
    return rv;
}

#! records an evaluation of a key of the path
string sub k(string tag) {
    print("  key " + tag + "\n");
    return tag;
}

#! the last index of the global list, read by a selector
int sub last_g() {
    printf("  read g %y\n", g);
    return g.size() - 1;
}

#! appends to the global list in a selector, and returns the index of the new element
int sub grow_g() {
    g += 99;
    printf("  grow g %y\n", g);
    return g.size() - 1;
}

#! adds a key to the global hash in a selector
int sub touch_gh() {
    gh.b = (9,);
    printf("  touch gh %y\n", gh);
    return 1;
}

#! runs a case and prints its result or its exception
sub t(string name, code<auto()> c) {
    printf("%s:\n", name);
    try {
        printf("  = %y\n", c());
    } catch (hash<ExceptionInfo> ex) {
        printf("  ! %s: %s\n", ex.err, ex.desc);
    }
}

sub main() {
    t("global list read", auto sub () { g = (1, 2, 3, 4); auto r = remove g[0, last_g()]; return (r, g); });
    t("global list write", auto sub () { g = (1, 2, 3, 4); auto r = remove g[0, grow_g()]; return (r, g); });
    t("global list range read", auto sub () { g = (1, 2, 3, 4); auto r = remove g[f("a", 0), f("b", 1)..last_g()];
        return (r, g); });
    t("global single range read", auto sub () { g = (1, 2, 3, 4); auto r = remove g[f("a", 1)..last_g()];
        return (r, g); });
    t("global single index read", auto sub () { g = (1, 2, 3, 4); auto r = remove g[last_g()]; return (r, g); });
    t("global delete read", auto sub () { g = (1, 2, 3, 4); delete g[0, last_g()]; return g; });
    t("global partial", auto sub () {
        g = (1, 2, 3, 4);
        try {
            remove g[f("a", 0), last_g(), kn..2];
        } catch (hash<ExceptionInfo> ex) {
            print("  caught " + ex.err + "\n");
        }
        return g;
    });
    t("global hash member", auto sub () {
        gh = {"a": (1, 2, 3, 4)};
        auto r = remove gh.(k("a"))[0, f("b", gh.a.size() - 1)];
        return (r, gh);
    });
    t("global hash member write", auto sub () {
        gh = {"a": (1, 2, 3, 4)};
        auto r = remove gh.a[touch_gh(), gh.size()];
        return (r, gh);
    });
    t("object member", auto sub () {
        o.l = (1, 2, 3, 4);
        auto r = remove o.l[0, f("b", o.l.size() - 1)];
        return (r, o.l);
    });
    t("object member write", auto sub () {
        o.l = (1, 2, 3, 4);
        auto r = remove o.l[0, f("b", (o.l += 5).size() - 1)];
        return (r, o.l);
    });
    t("static member", auto sub () {
        Holder::sl = (1, 2, 3, 4);
        auto r = remove Holder::sl[0, f("b", Holder::sl.size() - 1)];
        return (r, Holder::sl);
    });
    t("path order", auto sub () {
        gh = {"a": (1, 2, 3, 4)};
        auto r = remove gh.(k("a"))[f("x", 0), f("y", 1)..f("z", 2)];
        return (r, gh);
    });
    t("path order index", auto sub () {
        gh = {"a": (1, 2, 3, 4)};
        auto r = remove gh.(k("a"))[f("x", 1)];
        return (r, gh);
    });
    t("path order range", auto sub () {
        gh = {"a": (1, 2, 3, 4)};
        auto r = remove gh.(k("a"))[f("x", 1)..f("y", 2)];
        return (r, gh);
    });
    t("selector reads the list", auto sub () {
        list<auto> l = (1, 2, 3, 4);
        auto r = remove l[f("a", 0), l.size() - 1, l[0] ?? -1];
        return (r, l);
    });
    t("objects partial", auto sub () {
        go = (new Obj("x0"), new Obj("x1"), new Obj("x2"));
        try {
            delete go[f("a", 2), f("b", 0), kn..1];
        } catch (hash<ExceptionInfo> ex) {
            print("  caught " + ex.err + "\n");
        }
        return go.size();
    });
}

main();
