%modern
# the cases of selector-eval-once.qtest: lists of indices without a range and ranges that a foreach statement or a
# functional operator iterates, whose selectors and sequences are evaluated once (qore#5684)

# Copyright (C) 2026 Qore Technologies, s.r.o.

list<int> l = (10, 11, 12, 13);
string s = "abcd";
binary b = binary("abcd");

#! records an evaluation of a selector
int sub f(string tag, int rv) {
    print("  sel " + tag + "\n");
    return rv;
}

#! records an evaluation of a sequence
auto sub sq(string tag, auto v) {
    print("  seq " + tag + "\n");
    return v;
}

#! a selector that raises an exception
int sub thr(string tag) {
    print("  sel " + tag + "\n");
    throw "SEL-ERROR", tag;
}

#! the untyped indices that a JIT index access takes without the int48 fast path
const UntypedIndices = (1.5, "2", 1 << 50, NOTHING, True, -1, 3);

#! runs a case and prints its result or its exception
sub t(string name, code<auto()> c) {
    printf("%s:\n", name);
    try {
        printf("  = %y\n", c());
    } catch (hash<ExceptionInfo> ex) {
        printf("  ! %s\n", ex.err);
    }
}

# --- top-level code ---

print("top paren:\n");
foreach auto x in (l[(f("a", 0), f("b", 2))]) {
    printf("  body %y\n", x);
}
print("top list:\n");
foreach auto x in (l[f("a", 1), f("b", 3)]) {
    printf("  body %y\n", x);
}
print("top range empty:\n");
foreach auto x in (sq("l", l)[f("a", 7)..f("b", 9)]) {
    printf("  body %y\n", x);
}
print("top map:\n");
printf("  = %y\n", map $1 + 1, l[f("a", 3), f("b", 0)]);

# --- function bodies ---

sub main() {
    t("fe paren", auto sub () {
        foreach auto x in (l[(f("a", 0), f("b", 2))]) {
            printf("  body %y\n", x);
        }
        return "done";
    });
    t("fe list", auto sub () {
        foreach auto x in (l[f("a", 1), f("b", 3), f("c", 9)]) {
            printf("  body %y\n", x);
        }
        return "done";
    });
    t("fe list typed", auto sub () {
        foreach int x in (l[f("a", 1), f("b", 3)]) {
            printf("  body %y\n", x);
        }
        return "done";
    });
    t("map paren", auto sub () { return map $1 * 2, l[(f("a", 3), f("b", 1))]; });
    t("map list", auto sub () { return map $1 * 2, l[f("a", 3), f("b", 1)]; });
    t("select list", auto sub () { return select l[f("a", 0), f("b", 1), f("c", 2)], $1 % 2; });
    t("foldl list", auto sub () { return foldl $1 + $2, l[f("a", 0), f("b", 1), f("c", 2)]; });
    t("map index list", auto sub () { list<auto> ix = (f("a", 2), f("b", 0)); return map $1, l[ix]; });
    t("map range", auto sub () { return map $1, l[f("a", 1)..f("b", 2)]; });
    t("map range reverse", auto sub () { return map $1, l[f("a", 2)..f("b", 0)]; });
    t("map range empty", auto sub () { return map $1, sq("l", l)[f("a", 7)..f("b", 9)]; });
    t("foldl range empty", auto sub () { return foldl $1 + $2, sq("l", l)[f("a", 7)..f("b", 9)]; });
    t("fe range empty", auto sub () {
        foreach auto x in (sq("l", l)[f("a", 5)..f("b", 6)]) {
            printf("  body %y\n", x);
        }
        return "done";
    });
    t("map range negative", auto sub () { return map $1, l[f("a", -2)..f("b", -1)]; });
    t("map range string", auto sub () { return map $1, sq("s", s)[f("a", 0)..f("b", 1)]; });
    t("map range binary", auto sub () { return map $1, sq("b", b)[f("a", 1)..f("b", 2)]; });
    t("map range hash", auto sub () { return map $1, sq("h", {"a": 1})[f("a", 0)..f("b", 1)]; });
    t("plain index untyped", auto sub () {
        auto al = l;
        list<auto> r;
        foreach auto ix in (UntypedIndices) {
            push r, al[ix];
        }
        return r;
    });
    t("plain index untyped string", auto sub () {
        auto as = s;
        list<auto> r;
        foreach auto ix in (UntypedIndices) {
            push r, as[ix];
        }
        return r;
    });
    t("fe index untyped", auto sub () {
        auto al = l;
        foreach auto ix in (UntypedIndices) {
            foreach auto x in (al[ix]) {
                printf("  body %y\n", x);
            }
        }
        return "done";
    });
    t("map list untyped", auto sub () { auto al = sq("l", l); return map $1, al[f("a", 1), f("b", 9)]; });
    t("map range empty binary", auto sub () { return map $1, sq("e", binary())[f("a", 0)..f("b", 1)]; });
    t("fe range empty binary", auto sub () {
        foreach auto x in (sq("e", binary())[f("a", 0)..f("b", 1)]) {
            printf("  body %y\n", x);
        }
        return "done";
    });
    t("map list raises", auto sub () { return map $1, sq("l", l)[f("a", 0), thr("t")]; });
    t("fe list raises", auto sub () {
        foreach auto x in (sq("l", l)[f("a", 1), thr("t"), f("c", 2)]) {
            printf("  body %y\n", x);
        }
        return "done";
    });
    t("map range raises", auto sub () { return map $1, sq("l", l)[thr("t")..f("b", 2)]; });
    t("fe range raises", auto sub () {
        foreach auto x in (sq("l", l)[f("a", 0)..thr("t")]) {
            printf("  body %y\n", x);
        }
        return "done";
    });
    t("fe range string", auto sub () {
        foreach auto x in (sq("s", s)[f("a", 1)..f("b", 2)]) {
            printf("  body %y\n", x);
        }
        return "done";
    });
}

main();
