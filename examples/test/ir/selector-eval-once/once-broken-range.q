%new-style
%require-types
%strict-args
%broken-list-range
# the cases of selector-eval-once.qtest with %broken-list-range: ranges that a foreach statement or a functional
# operator iterates, whose bounds and sequences are evaluated once (qore#5684)
#
# %broken-list-range is a compatibility option, which %modern refuses (qore#5682), so this script is legacy code: it
# runs on the AST tier in every execution mode and is not compiled by qcc; selector-eval-once.qtest checks it with the
# AST interpreter

# Copyright (C) 2026 Qore Technologies, s.r.o.

# an element type that allows NOTHING: a range past the end of a list<int> raises a type error with %broken-list-range (qore#5757)
list<*int> l = (10, 11, 12, 13);

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

#! runs a case and prints its result or its exception
sub t(string name, code<auto()> c) {
    printf("%s:\n", name);
    try {
        printf("  = %y\n", c());
    } catch (hash<ExceptionInfo> ex) {
        printf("  ! %s\n", ex.err);
    }
}

sub main() {
    t("map range", auto sub () { return map $1, sq("l", l)[f("a", 0)..f("b", 2)]; });
    t("map range reverse", auto sub () { return map $1, sq("l", l)[f("a", 3)..f("b", 1)]; });
    t("map range empty", auto sub () { return map $1, sq("l", l)[f("a", 7)..f("b", 9)]; });
    t("map range string", auto sub () { return map $1, sq("s", "abcd")[f("a", 1)..f("b", 3)]; });
    t("fe range", auto sub () {
        foreach auto x in (sq("l", l)[f("a", 1)..f("b", 3)]) {
            printf("  body %y\n", x);
        }
        return "done";
    });
    t("fe range binary", auto sub () {
        foreach auto x in (sq("b", binary("abcd"))[f("a", 0)..f("b", 2)]) {
            printf("  body %y\n", x);
        }
        return "done";
    });
}

main();
