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

#! an object whose members changed from outside the class are reported
class Notified {
    public {
        list<auto> l = (1, 2, 3, 4);
    }

    memberNotification(string m) {
        print("  notify " + m + "\n");
    }

    #! removes from a member of self with a selector that raises an exception
    auto removeSelf() {
        return remove l[thr("self")];
    }
}

our Notified gn = new Notified();
#! a deleted object, which is still referenced by this global
our auto gd;
thread_local list<auto> tl = (1, 2, 3, 4);

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

#! a selector that raises an exception
int sub thr(string tag) {
    print("  sel throw " + tag + "\n");
    throw "SEL-ERROR", tag;
}

#! runs a removal whose selector raises an exception and prints the exceptions raised
sub caught(code<auto()> c) {
    try {
        c();
        print("  no exception\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("  caught %s: %s%s\n", ex.err, ex.desc, ex.next ? " then " + ex.next.err : "");
    }
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
    # a selector that raises an exception before anything is selected: the lvalue is not resolved or locked
    t("raise single member notification", auto sub () {
        gn.l = (1, 2, 3, 4);
        caught(auto sub () { return remove gn.l[thr("single")]; });
        return gn.l;
    });
    t("raise range member notification", auto sub () {
        gn.l = (1, 2, 3, 4);
        caught(auto sub () { return remove gn.l[f("a", 0)..thr("range")]; });
        return gn.l;
    });
    t("raise first list member notification", auto sub () {
        gn.l = (1, 2, 3, 4);
        caught(auto sub () { return remove gn.l[thr("first"), 1]; });
        return gn.l;
    });
    t("raise delete member notification", auto sub () {
        gn.l = (1, 2, 3, 4);
        caught(auto sub () { delete gn.l[thr("delete")]; });
        return gn.l;
    });
    t("raise deleted object", auto sub () {
        {
            Holder h = new Holder();
            gd = h;
            delete h;
        }
        caught(auto sub () { return remove gd.l[thr("single")]; });
        caught(auto sub () { return remove gd.l[thr("range")..2]; });
        caught(auto sub () { return remove gd.l[thr("first"), 1]; });
        return exists gd;
    });
    t("raise later member notification", auto sub () {
        gn.l = (1, 2, 3, 4);
        # what the first selector selected is removed from the resolved lvalue, which is reported
        caught(auto sub () { return remove gn.l[f("a", 0), thr("second")]; });
        return gn.l;
    });
    t("raise later deleted object", auto sub () {
        # the lvalue is resolved for the removal of what the first selector selected
        caught(auto sub () { return remove gd.l[f("a", 0), thr("second")]; });
        return exists gd;
    });
    t("raise static member", auto sub () {
        Holder::sl = (1, 2, 3, 4);
        caught(auto sub () { return remove Holder::sl[thr("single")]; });
        caught(auto sub () { return remove Holder::sl[thr("range")..2]; });
        caught(auto sub () { return remove Holder::sl[thr("first"), 1]; });
        return Holder::sl;
    });
    t("raise behind path key", auto sub () {
        gh = {"a": (1, 2, 3, 4)};
        caught(auto sub () { return remove gh.(k("a"))[thr("single")]; });
        caught(auto sub () { return remove gh.(k("a"))[thr("range")..2]; });
        return gh;
    });
    t("raise path key", auto sub () {
        gh = {"a": (1, 2, 3, 4)};
        caught(auto sub () { return remove gh.(thr("key").toString())[f("never", 0)]; });
        return gh;
    });
    t("raise self member", auto sub () {
        gn.l = (1, 2, 3, 4);
        caught(\gn.removeSelf());
        return gn.l;
    });
    t("raise reference", auto sub () {
        g = (1, 2, 3, 4);
        reference<list<auto>> r = \g;
        caught(auto sub () { return remove r[thr("single")]; });
        caught(auto sub () { return remove r[f("a", 0), thr("second")]; });
        return g;
    });
    t("raise closure variable", auto sub () {
        list<auto> cl = (1, 2, 3, 4);
        code c = auto sub () { return remove cl[thr("single")]; };
        caught(c);
        caught(auto sub () { return remove cl[f("a", 1)..thr("range")]; });
        return cl;
    });
    t("raise thread local", auto sub () {
        tl = (1, 2, 3, 4);
        caught(auto sub () { return remove tl[thr("single")]; });
        caught(auto sub () { return remove tl[f("a", 0), thr("second")]; });
        return tl;
    });
}

main();
