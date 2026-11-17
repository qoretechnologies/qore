%new-style
%require-types
%strict-args
%broken-list-range
# the cases of broken-list-range-past-end.qtest (qore#5757): with %broken-list-range, a range of a list that reaches
# outside of the list gives NOTHING for each element outside of it, in a slice whose element type allows NOTHING
#
# %broken-list-range is a compatibility option, which %modern refuses (qore#5682), so this script is legacy code, which
# only the AST interpreter runs

# Copyright (C) 2026 Qore Technologies, s.r.o.

sub t(string label, code c) {
    try {
        auto v = c();
        printf("%s: %y %s\n", label, v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

list<int> l = (10, 11, 12, 13);
list<auto> la = (1, 2);
list lu = (1, 2);
list<string> ls = ("a", "b");

t("int past end", auto sub () { return l[7..9]; });
t("int map past end", auto sub () { return map $1, l[7..9]; });
t("int map across end", auto sub () { return map $1, l[2..5]; });
t("int across end", auto sub () { return l[2..5]; });
t("int reverse across end", auto sub () { return l[5..2]; });
t("int in range", auto sub () { return l[1..2]; });
t("int before start", auto sub () { return l[-3..-1]; });
t("int across start", auto sub () { return l[-2..1]; });
t("int selector across end", auto sub () { return l[(2..5)]; });
t("int foreach across end", auto sub () { list<auto> r = (); foreach auto v in (l[3..5]) { push r, v; } return r; });
t("int select across end", auto sub () { return select l[2..5], True; });
t("softlist across end", auto sub () { softlist<int> sl = l; return sl[3..4]; });
t("string across end", auto sub () { return ls[1..3]; });
t("auto across end", auto sub () { return la[0..3]; });
t("untyped across end", auto sub () { return lu[0..3]; });
t("empty int list", auto sub () { list<int> e = (); return e[0..1]; });
t("or-nothing list across end", auto sub () { *list<int> ol = l; return ol[3..4]; });
# a slice in range is a list<int> as before; one outside of it is not, which an assignment to a list<int> checks
t("in range to list<int>", auto sub () { list<int> x = l[1..2]; return x; });
t("across end to list<int>", auto sub () { list<int> x = l[2..5]; return x; });
