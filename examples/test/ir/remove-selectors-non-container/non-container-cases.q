%modern
%disable-warning invalid-operation
# the cases of remove-selectors-non-container.qtest: remove and delete with a list of selectors on a value that is
# not a list, a string or a binary object evaluate the selectors in order and remove nothing (qore#5695)

# Copyright (C) 2026 Qore Technologies, s.r.o.

class Obj {
    public {
        string name;
        *hash<auto> h;
    }

    constructor(string n) {
        name = n;
    }

    destructor() {
        print("  destroy " + name + "\n");
    }

    memberNotification(string m) {
        print("  notify " + m + "\n");
    }
}

our auto g;
our Obj gn = new Obj("gn");

#! records an evaluation of a selector
int sub f(string tag, int rv) {
    print("  sel " + tag + "\n");
    return rv;
}

#! a selector that raises an exception
int sub thr(string tag) {
    print("  sel throw " + tag + "\n");
    throw "SEL-ERROR", tag;
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
    # a local
    t("remove hash", auto sub () { hash<auto> h = {"a": 1, "b": 2}; auto r = remove h[f("a", 0), f("b", 1)];
        return (r, h); });
    t("remove hash range", auto sub () { hash<auto> h = {"a": 1}; auto r = remove h[f("a", 0), f("b", 1)..f("c", 2)];
        return (r, h); });
    t("delete hash", auto sub () { hash<auto> h = {"a": 1}; delete h[f("a", 0), f("b", 1)]; return h; });
    t("remove int", auto sub () { auto i = 5; auto r = remove i[f("a", 0), f("b", 1)]; return (r, i); });
    t("delete int", auto sub () { auto i = 5; delete i[f("a", 0), f("b", 1)..f("c", 2)]; return i; });
    t("remove float", auto sub () { auto v = 1.5; auto r = remove v[f("a", 0), f("b", 1)]; return (r, v); });
    t("remove object", auto sub () {
        auto o = new Obj("lo");
        auto r = remove o[f("a", 0), f("b", 1)];
        return (r, o.name);
    });
    t("delete object", auto sub () {
        auto o = new Obj("ld");
        delete o[f("a", 0), f("b", 1)];
        return o.name;
    });
    t("remove nothing", auto sub () { auto n; auto r = remove n[f("a", 0), f("b", 1)]; return (r, n); });
    t("delete nothing", auto sub () { auto n; delete n[f("a", 0), f("b", 1)..f("c", 2)]; return n; });
    # a global
    t("remove global hash", auto sub () { g = {"a": 1}; auto r = remove g[f("a", 0), f("b", 1)]; return (r, g); });
    t("delete global int", auto sub () { g = 7; delete g[f("a", 0), f("b", 1)]; return g; });
    # a member of an object, which is reported only if it changes
    t("remove member hash", auto sub () {
        gn.h = {"a": 1};
        auto r = remove gn.h[f("a", 0), f("b", 1)];
        return (r, gn.h);
    });
    # a later selector that raises an exception: nothing is removed, the exception is raised
    t("raise later hash", auto sub () {
        hash<auto> h = {"a": 1};
        try {
            remove h[f("a", 0), thr("second")];
        } catch (hash<ExceptionInfo> ex) {
            print("  caught " + ex.err + "\n");
        }
        return h;
    });
    t("raise later int", auto sub () {
        auto i = 3;
        try {
            delete i[f("a", 0), thr("second"), f("c", 2)];
        } catch (hash<ExceptionInfo> ex) {
            print("  caught " + ex.err + "\n");
        }
        return i;
    });
    # a single index still removes the value as a whole
    t("single index hash", auto sub () { hash<auto> h = {"a": 1}; auto r = remove h[f("a", 0)]; return (r, h); });
    t("single index int", auto sub () { auto i = 5; auto r = remove i[f("a", 0)]; return (r, i); });
    # a list, a string and a binary object still give the selected elements
    t("list", auto sub () { list<auto> l = (1, 2, 3); auto r = remove l[f("a", 0), f("b", 2)]; return (r, l); });
    t("string", auto sub () { string s = "abc"; auto r = remove s[f("a", 0), f("b", 2)]; return (r, s); });
}

main();
