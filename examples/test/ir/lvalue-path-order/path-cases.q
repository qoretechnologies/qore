%modern
# the cases of lvalue-path-order.qtest: the keys and indices of a path of two or more steps are evaluated from the
# root out, and then the index of the last step, for every kind of lvalue and root (qore#5753)

# Copyright (C) 2026 Qore Technologies, s.r.o.

our hash<auto> gh;
thread_local hash<auto> tlh;

class C {
    public {
        hash<auto> m;
        static hash<auto> sh;
    }

    #! assigns to a member of self through a path
    setSelf() {
        m.(k("a")).(k("b"))[i("0", 1)] = v("r", 7);
    }
}

C o();

#! records the evaluation of a key
string sub k(string t) {
    print(" k" + t);
    return t;
}

#! records the evaluation of an index
int sub i(string t, int x) {
    print(" i" + t);
    return x;
}

#! records the evaluation of a value
auto sub v(string t, auto x) {
    print(" v" + t);
    return x;
}

#! a key that raises an exception
string sub thr(string t) {
    print(" throw" + t);
    throw "KEY-ERROR", t;
}

#! a fresh value for a path
hash<auto> sub mk() {
    return {"a": {"b": (1, 2, 3), "c": 1}};
}

#! runs a case and prints the evaluations and the result or the exception
sub t(string name, code<auto()> c) {
    gh = mk();
    printf("%s:", name);
    try {
        auto r = c();
        printf(" = %y", r);
    } catch (hash<ExceptionInfo> ex) {
        printf(" ! %s", ex.err);
    }
    print("\n");
}

sub main() {
    # the kinds of change
    t("assign", auto sub () { gh.(k("a")).(k("b"))[i("0", 1)] = v("r", 9); return gh.a.b; });
    t("compound", auto sub () { gh.(k("a")).(k("b"))[i("0", 1)] += v("r", 9); return gh.a.b; });
    t("preinc", auto sub () { ++gh.(k("a")).(k("b"))[i("0", 1)]; return gh.a.b; });
    t("postdec", auto sub () { gh.(k("a")).(k("b"))[i("0", 1)]--; return gh.a.b; });
    t("remove", auto sub () { auto r = remove gh.(k("a")).(k("b"))[i("0", 1)]; return (r, gh.a.b); });
    t("delete", auto sub () { delete gh.(k("a")).(k("b"))[i("0", 1)]; return gh.a.b; });
    t("remove key", auto sub () { auto r = remove gh.(k("a")).(k("c")); return (r, gh.a); });
    t("delete key", auto sub () { delete gh.(k("a")).(k("c")); return gh.a; });
    t("remove list", auto sub () { auto r = remove gh.(k("a")).(k("b"))[i("0", 0), i("1", 1)]; return (r, gh.a.b); });
    t("splice", auto sub () { splice gh.(k("a")).(k("b")), i("0", 0), i("1", 1); return gh.a.b; });
    t("extract", auto sub () { auto r = extract gh.(k("a")).(k("b")), i("0", 0), i("1", 1); return (r, gh.a.b); });
    t("push", auto sub () { push gh.(k("a")).(k("b")), v("r", 4); return gh.a.b; });
    t("unshift", auto sub () { unshift gh.(k("a")).(k("b")), v("r", 0); return gh.a.b; });
    t("pop", auto sub () { auto r = pop gh.(k("a")).(k("b")); return (r, gh.a.b); });
    t("reference", auto sub () {
        reference r = \gh.(k("a")).(k("b"))[i("0", 1)];
        print(" made");
        r = 5;
        return gh.a.b;
    });
    t("three keys", auto sub () { gh.(k("a")).(k("x")).(k("y")) = v("r", 1); return gh.a.x; });
    t("constant keys", auto sub () { gh.a.b[i("0", 2)] = v("r", 8); return gh.a.b; });
    # inside map and foreach
    t("map", auto sub () { return map (gh.(k("a")).(k("b"))[i("0", $1)] = v("r", $1)), (0, 1); });
    t("foreach", auto sub () {
        foreach int x in (0, 1) {
            gh.(k("a")).(k("b"))[i("0", x)] = v("r", x);
        }
        return gh.a.b;
    });
    # the kinds of root
    t("local", auto sub () { hash<auto> h = mk(); h.(k("a")).(k("b"))[i("0", 1)] = v("r", 9); return h.a.b; });
    t("member", auto sub () { o.m = mk(); o.m.(k("a")).(k("b"))[i("0", 1)] = v("r", 9); return o.m.a.b; });
    t("self member", auto sub () { o.m = mk(); o.setSelf(); return o.m.a.b; });
    t("static", auto sub () { C::sh = mk(); C::sh.(k("a")).(k("b"))[i("0", 1)] = v("r", 9); return C::sh.a.b; });
    t("thread local", auto sub () { tlh = mk(); tlh.(k("a")).(k("b"))[i("0", 1)] = v("r", 9); return tlh.a.b; });
    t("closure", auto sub () {
        hash<auto> h = mk();
        code c = sub () { h.(k("a")).(k("b"))[i("0", 1)] = v("r", 9); };
        c();
        return h.a.b;
    });
    t("reference root", auto sub () {
        reference<hash<auto>> r = \gh;
        r.(k("a")).(k("b"))[i("0", 1)] = v("r", 9);
        return gh.a.b;
    });
    # a key that raises an exception: the later keys are not evaluated and nothing is changed
    t("raise", auto sub () { gh.(k("a")).(thr("b")).(k("c")) = v("r", 1); return gh; });
    t("raise remove", auto sub () { remove gh.(k("a")).(thr("b"))[i("0", 0)]; return gh; });
    # a long path
    t("long path", auto sub () {
        hash<auto> h;
        h.(k("1")).(k("2")).(k("3")).(k("4")).(k("5")).(k("6")).(k("7")).(k("8")).(k("9")).(k("10")).(k("11"))
            .(k("12")).(k("13")).(k("14")).(k("15")).(k("16")).(k("17")).(k("18")) = 1;
        return h.size();
    });
}

main();
