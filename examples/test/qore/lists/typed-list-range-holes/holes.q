%modern
# the cases of typed-list-range-holes.qtest (qore#6435): a range of a typed list whose element type does not take
# NOTHING that holds a hole (left by delete or remove) or a gap (filled by an assignment past the end of the list)
# gives a slice with the or-nothing element type; it failed an assertion in a debug build in every execution mode

# Copyright (C) 2026 Qore Technologies, s.r.o.

sub t(string label, code c) {
    try {
        auto v = c();
        printf("%s: %y %s\n", label, v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

#! a list<int> with a hole at index 1 left by delete
list<int> sub delete_hole() {
    list<int> l = (10, 11, 12, 13);
    delete l[1];
    return l;
}

#! a list<int> with a hole at index 0 left by remove
list<int> sub remove_hole() {
    list<int> l = (10, 11, 12, 13);
    remove l[0];
    return l;
}

#! a list<int> with a gap at indexes 1 and 2 filled by an assignment past its end
list<int> sub gap() {
    list<int> l = (10,);
    l[3] = 13;
    return l;
}

t("delete hole", \delete_hole());
t("delete hole slice", auto sub () { list<int> l = delete_hole(); return l[0..2]; });
t("delete hole reverse slice", auto sub () { list<int> l = delete_hole(); return l[3..0]; });
t("delete hole slice without the hole", auto sub () { list<int> l = delete_hole(); return l[2..3]; });
t("remove hole slice", auto sub () { list<int> l = remove_hole(); return l[0..1]; });
t("gap", \gap());
t("gap slice", auto sub () { list<int> l = gap(); return l[0..3]; });
t("gap slice of the gap", auto sub () { list<int> l = gap(); return l[1..2]; });
t("gap slice in a variable", auto sub () { list<int> l = gap(); auto s = l[0..3]; return s; });
# map and select build their lists with the element type of the list, as over a list with a hole (not a range);
# select is left out: the AST raises a RUNTIME-TYPE-ERROR, and the other modes give 0 for each hole (qore#6437)
t("gap map", auto sub () { list<int> l = gap(); return map $1, l[0..3]; });
t("gap foreach", auto sub () {
    list<int> l = gap();
    list<auto> r = ();
    foreach auto v in (l[0..3]) {
        push r, v;
    }
    return r;
});
t("gap selector range", auto sub () { list<int> l = gap(); return l[(0..3)]; });
t("string gap slice", auto sub () { list<string> l = ("a",); l[2] = "c"; return l[0..2]; });
t("softint gap slice", auto sub () { list<softint> l = (10,); l[2] = 12; return l[0..2]; });
t("or-nothing gap slice", auto sub () { list<*int> l = (10,); l[2] = 12; return l[0..2]; });
t("hash member gap slice", auto sub () {
    hash<string, list<int>> h = {"a": (10,)};
    h.a[2] = 12;
    return h.a[0..2];
});
# a slice with a hole is not a list<int>, which an assignment to a list<int> checks
t("gap slice to list<int>", auto sub () { list<int> l = gap(); list<int> s = l[0..3]; return s; });
