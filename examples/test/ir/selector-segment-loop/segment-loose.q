%modern
%loose-types
# the cases of selector-segment-loop.qtest with %loose-types: a list-typed variable that holds NOTHING, whose
# selectors are not evaluated and which gives no value, in every execution mode (qore#5711)

# Copyright (C) 2026 Qore Technologies, s.r.o.

class Holder {
    public {
        list<int> li;
        list<float> lf;
        list<bool> lb;
    }
}

hashdecl HoldsList {
    list<int> li;
}

#! records an evaluation of a selector
int sub f(string tag, int rv) {
    print("  sel " + tag + "\n");
    return rv;
}

#! an impure consumer of a value that can be NOTHING
int sub g(*int v) {
    return v ?? 0;
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
    t("fe local stable", auto sub () {
        list<int> l;
        int n = 0;
        foreach auto x in (l[0..2]) {
            printf("  body %y\n", x);
        }
        foreach auto x in (l[n, n..n + 2]) {
            printf("  body %y\n", x);
        }
        return l;
    });
    t("fe local selectors", auto sub () {
        list<int> l;
        foreach auto x in (l[f("a", 0), f("b", 1)..f("c", 2)]) {
            printf("  body %y\n", x);
        }
        return "done";
    });
    t("map local", auto sub () { list<int> l; return map $1 + 1, l[f("a", 0), f("b", 1)..f("c", 2)]; });
    t("map local stable", auto sub () { list<int> l; int n = 1; return map $1, l[0, n..2]; });
    t("select local", auto sub () { list<int> l; return select l[f("a", 0)..f("b", 2)], $1 > 0; });
    t("foldl local", auto sub () { list<int> l; return foldl $1 + $2, l[f("a", 0), f("b", 1)..f("c", 2)]; });
    # impure consumers: the lazy pipeline of a list of selectors, which a sequence that is NOTHING does not enter
    t("map impure", auto sub () { list<int> l; return map g($1), l[f("a", 0), f("b", 1)..f("c", 2)]; });
    t("select impure", auto sub () { list<int> l; return select l[f("a", 0), f("b", 1)..f("c", 2)], g($1) > 0; });
    t("foldl impure", auto sub () { list<int> l; return foldl $1 + g($2), l[f("a", 0), f("b", 1)..f("c", 2)]; });
    # stable selectors with a range, iterated lazily
    t("fe stable lazy", auto sub () {
        list<int> l;
        int n = 0;
        foreach auto x in (l[n, n..n + 2]) {
            printf("  body %y\n", x);
        }
        return "done";
    });
    t("map stable lazy", auto sub () { list<int> l; int n = 1; return map g($1), l[0, n..2]; });
    t("fe member float", auto sub () {
        Holder h();
        foreach auto x in (h.lf[f("a", 0)..f("b", 1)]) {
            printf("  body %y\n", x);
        }
        return "done";
    });
    t("map member bool", auto sub () { Holder h(); return map $1, h.lb[f("a", 0), f("b", 1)]; });
    t("map member int stable", auto sub () { Holder h(); return map $1, h.li[0..3]; });
    t("fe hashdecl member", auto sub () {
        hash<HoldsList> hl();
        foreach auto x in (hl.li[f("a", 0), f("b", 1)..f("c", 2)]) {
            printf("  body %y\n", x);
        }
        return "done";
    });
    # the same variable once it holds a list
    t("fe local assigned", auto sub () {
        list<int> l;
        l = (10, 11, 12);
        foreach auto x in (l[f("a", 0), f("b", 1)..f("c", 2)]) {
            printf("  body %y\n", x);
        }
        return "done";
    });
}

main();
