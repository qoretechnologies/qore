%modern
%allow-weak-references
# the [] operator with a string key on a hash, which the parser warns about, is tested
%disable-warning invalid-operation
# the cases of typed-list-holes.qtest (qore#6435, qore#6437): a list whose element type does not take NOTHING never
# holds NOTHING: an assignment past its end that would leave a gap raises RUNTIME-TYPE-ERROR, by the type of the list
# at run time, and delete or remove of an element makes the list a list<*T> if the declared type of the lvalue holding
# it allows it, and raises RUNTIME-TYPE-ERROR if it requires the element type

# Copyright (C) 2026 Qore Technologies, s.r.o.

sub t(string label, code c) {
    try {
        auto v = c();
        printf("%s: %y %s\n", label, v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

#! deletes the element at index 1 of a list and returns the list; compiled to native code in JIT and tiered modes
auto sub del1(reference<auto> r) {
    delete r[1];
    return r;
}

#! deletes the element at index 0 of a list<int> through a reference parameter that requires the type
auto sub del_int(reference<list<int>> r) {
    delete r[0];
    return r;
}

#! deletes an element of the list<int> in a hash through a reference parameter that requires the value type
auto sub del_hash(reference<hash<string, list<int>>> r) {
    delete r.a[0];
    return r;
}

#! deletes an element of the list<int> in a list through a reference parameter that requires the element type
auto sub del_nested(reference<list<list<int>>> r) {
    delete r[0][0];
    return r;
}

#! deletes an element of an auto member of an object named by a reference parameter
auto sub del_obj(reference<Holder> r) {
    delete r.a[0];
    return r.a;
}

#! passes a reference<list<int>> parameter on as a reference<auto>, whose type is not noted (a reference<list<int>>
#! cannot be passed on to a parameter of another typed reference type): the type of the inner reference decides
auto sub del_outer_list(reference<list<int>> r) {
    return del_list(\r);
}

#! deletes the element at index 0 of a list named by a reference<auto>
auto sub del_list(reference<auto> r) {
    delete r[0];
    return r;
}

#! deletes an element of a list in a hash named by a reference<hash<auto>>
auto sub del_hash_auto(reference<hash<auto>> r) {
    delete r.a[0];
    return r;
}

#! appends to a list<int> named by a reference<list<int>> passed on from another reference parameter
sub ref_append(reference<list<int>> r) {
    r += 3;
}
sub ref_pass_on(reference<list<int>> r) {
    ref_append(\r);
}

#! removes the element at index 1 of a list held by an auto parameter and returns the list
auto sub rem1(auto l) {
    auto v = remove l[1];
    return (v, l);
}

#! a select over a list, which JIT and tiered execution compile to native code
list<auto> sub sel_all(auto l) {
    return select l, True;
}

hashdecl HL {
    list<int> m;
}

class Holder {
    public {
        auto a;
        list<int> i = ();
    }
}

our auto ga;
our list<int> gi = ();

#! a select over a list<int>, which JIT and tiered execution compile to native code
list<auto> sub select_ints(list<int> l) {
    return select l, $1 > 10;
}

# assignments past the end
t("gap", auto sub () { list<int> l = (1,); l[3] = 4; return l; });
t("gap leaves the list", auto sub () { list<int> l = (1,); try { l[3] = 4; } catch () {} return l; });
t("append", auto sub () { list<int> l = (1,); l[1] = 4; return l; });
t("append to an empty list", auto sub () { list<int> l = (); l[0] = 4; return l; });
t("gap in an empty list", auto sub () { list<int> l = (); l[1] = 4; return l; });
t("gap in a new list", auto sub () { *list<int> l; l[2] = 4; return l; });
t("append to a new list", auto sub () { *list<int> l; l[0] = 4; return l; });
t("gap +=", auto sub () { list<int> l = (1,); l[3] += 4; return l; });
t("gap += leaves the list", auto sub () { list<int> l = (1,); try { l[3] += 4; } catch () {} return l; });
t("gap *=", auto sub () { list<int> l = (1,); l[3] *= 4; return l; });
t("gap ++", auto sub () { list<int> l = (1,); l[3]++; return l; });
t("gap -=", auto sub () { list<int> l = (1,); l[3] -= 4; return l; });
t("gap string", auto sub () { list<string> l = ("a",); l[2] = "c"; return l; });
t("gap float", auto sub () { list<float> l = (1.5,); l[2] = 2.5; return l; });
t("gap hash member", auto sub () { hash<string, list<int>> h = {"a": (1,)}; h.a[2] = 3; return h; });
t("gap nested list", auto sub () { list<list<int>> l = ((1,),); l[2][0] = 4; return l; });
t("gap inner list", auto sub () { list<list<int>> l = ((1,),); l[0][2] = 4; return l; });
t("gap new key leaves the hash", auto sub () {
    hash<string, list<int>> h = {"a": (1,)};
    try { h.b[2] = 3; } catch () {}
    return h;
});
# the type of the list at run time: a list of ints held by auto or list<auto> is a list<int>
t("gap auto", auto sub () { auto l = (1, 2); l[5] = 3; return l; });
t("gap list<auto>", auto sub () { list<auto> l = (1, 2); l[5] = 3; return l; });
t("reference read past the end", auto sub () { list<int> l = (1,); reference r = \l[3]; auto x = r; return (x, l); });
t("reference read past the end untyped", auto sub () {
    list l = (1,);
    reference r = \l[3];
    auto x = r;
    return (x, l);
});
t("reference write at the end", auto sub () { list<int> l = (1,); reference r = \l[1]; r = 2; return l; });
# lists whose element type takes NOTHING are padded
t("gap or-nothing", auto sub () { list<*int> l = (1,); l[3] = 4; return l; });
t("gap untyped", auto sub () { list l = (1,); l[3] = 4; return l; });
t("gap untyped +=", auto sub () { list l = (1,); l[3] += 4; return l; });
t("gap mixed auto", auto sub () { auto l = (1, "a"); l[3] = 4; return l; });

# delete and remove of an element of a list whose element type does not take NOTHING: the list becomes a list<*T> if
# the declared type of the lvalue holding it allows it, and RUNTIME-TYPE-ERROR is raised if it requires the type
t("delete declared list<int>", auto sub () { list<int> l = (10, 11, 12); delete l[1]; return l; });
t("delete leaves the list", auto sub () { list<int> l = (10, 11, 12); try { delete l[1]; } catch () {} return l; });
t("remove declared list<int>", auto sub () { list<int> l = (10, 11, 12); remove l[1]; return l; });
t("delete declared list<float>", auto sub () { list<float> l = (1.5, 2.5); delete l[0]; return l; });
t("delete declared list<string>", auto sub () { list<string> l = ("a", "b"); delete l[0]; return l; });
t("delete declared softlist<int>", auto sub () { softlist<int> l = (1, 2); delete l[0]; return l; });
t("delete declared *list<int>", auto sub () { *list<int> l = (1, 2); delete l[0]; return l; });
t("delete auto", auto sub () { auto l = (10, 11, 12); delete l[1]; return l; });
t("remove auto", auto sub () { auto l = (10, 11, 12); auto v = remove l[1]; return (v, l); });
t("delete list<auto>", auto sub () { list<auto> l = (1.5, 2.5); delete l[0]; return l; });
t("delete list", auto sub () { list l = (1, 2); delete l[0]; return l; });
t("delete declared list<*int>", auto sub () { list<*int> l = (1, 2); delete l[0]; return l; });
t("delete auto keeps a copy", auto sub () { auto l = (10, 11, 12); auto c = l; delete l[1]; return (c, l); });
t("delete auto to list<int>", auto sub () { auto l = (10, 11, 12); delete l[1]; list<int> l2 = l; return l2; });
t("delete auto then assign", auto sub () { auto l = (10, 11, 12); delete l[1]; l[1] = 5; return l; });
t("delete auto then gap", auto sub () { auto l = (10, 11); delete l[1]; l[3] = 5; return l; });
t("delete last auto", auto sub () { auto l = (10, 11, 12); delete l[2]; return l; });
t("delete auto objects", auto sub () { auto l = (new Mutex(), new Mutex()); delete l[0]; return l.fullType(); });
t("delete hash<auto> value", auto sub () { hash<auto> h = {"a": (1, 2)}; delete h.a[0]; return h.a; });
t("delete declared hash value", auto sub () { hash<string, list<int>> h = {"a": (1, 2)}; delete h.a[0]; return h; });
t("delete nested declared", auto sub () { list<list<int>> l = ((1, 2),); delete l[0][1]; return l; });
t("delete nested auto", auto sub () { auto l = ((1, 2),); delete l[0][1]; return l; });
t("delete outer declared", auto sub () { list<list<int>> l = ((1,), (2,)); delete l[0]; return l; });
t("delete outer auto", auto sub () { auto l = ((1,), (2,)); delete l[0]; return l; });
t("delete through a reference", auto sub () { auto l = (1, 2); reference r = \l[0]; delete r; return l; });
t("delete through a reference to list<int>", auto sub () {
    list<int> l = (1, 2);
    reference r = \l[0];
    delete r;
    return l;
});
t("delete in a function", auto sub () { auto l = (1, 2, 3); return del1(\l); });
t("delete in a function list<int> param", auto sub () { list<int> l = (1, 2, 3); return del_int(\l); });
t("delete in a function list<int> param of auto", auto sub () { auto l = (1, 2, 3); return del_int(\l); });
t("delete outer then inner", auto sub () {
    auto l = ((1,), (2,));
    delete l[0];
    delete l[1][0];
    return (l, l.fullType(), l[0..1]);
});
t("delete hash with [] auto", auto sub () {
    auto h = {"a": (1, 2)};
    auto k = "a";
    delete h[k][0];
    return (h, h.fullType());
});
t("delete hash with [] declared", auto sub () {
    hash<string, list<int>> h = {"a": (1, 2)};
    auto k = "a";
    delete h[k][0];
    return h;
});
t("delete hashdecl member with []", auto sub () {
    list<auto> l = (<HL>{"m": (1, 2)},);
    auto k = "m";
    delete l[0][k][0];
    return l;
});
t("delete hashdecl member", auto sub () { list<auto> l = (<HL>{"m": (1, 2)},); delete l[0].m[0]; return l; });
t("delete in a hash<string, list<auto>> of auto", auto sub () {
    auto h = cast<hash<string, list<auto>>>({"a": (1, 2), "b": ("x",)});
    delete h.a[0];
    return (h, h.fullType(), h.a.fullType(), h.b.fullType());
});
t("delete in a list<list<auto>> of auto", auto sub () {
    auto l = cast<list<list<auto>>>(((1, 2), ("x",)));
    delete l[0][0];
    return (l, l.fullType(), l[0].fullType(), l[1].fullType());
});
t("delete in a list<softlist<int>> of auto", auto sub () {
    auto l = cast<list<softlist<int>>>(((1, 2),));
    on_exit printf("  l: %y %s %s\n", l, l.fullType(), l[0].fullType());
    delete l[0][0];
    return l;
});
# the cast to list<list> makes the inner list an untyped list, which takes NOTHING: nothing is widened or retyped
t("delete in a list<list> in a list of auto", auto sub () {
    auto l = (cast<list<list>>(((1, 2),)),);
    delete l[0][0][0];
    return (l, l.fullType(), l[0].fullType(), l[0][0].fullType());
});
t("delete through a weak reference", auto sub () {
    auto h = cast<hash<string, list<int>>>({"a": (1, 2)});
    auto w := h;
    on_exit printf("  h: %y %s\n", h, h.fullType());
    delete w.a[0];
    return (h, h.fullType());
});
t("delete in a hash through a reference<hash<string, list<int>>> of auto", auto sub () {
    auto h = cast<hash<string, list<int>>>({"a": (1, 2)});
    on_exit printf("  h: %y %s\n", h, h.fullType());
    return del_hash(\h);
});
t("delete in a nested list through a reference<list<list<int>>> of auto", auto sub () {
    auto l = cast<list<list<int>>>(((1, 2),));
    on_exit printf("  l: %y %s\n", l, l.fullType());
    return del_nested(\l);
});
t("delete in an object member through a reference<Holder>", auto sub () {
    hash<auto> h = {"o": new Holder()};
    h.o.a = (1, 2);
    return del_obj(\h.o);
});
t("delete through nested references reference<auto> of reference<list<int>>", auto sub () {
    auto l = (1, 2);
    on_exit printf("  l: %y %s\n", l, l.fullType());
    return del_outer_list(\l);
});
t("delete in a hash through a reference<hash<auto>> of auto", auto sub () {
    auto h = {"a": (1, 2)};
    del_hash_auto(\h);
    return (h, h.fullType());
});
t("typed reference passed on", auto sub () { auto l = (1, 2); ref_pass_on(\l); return l; });
t("remove in a function", auto sub () { return rem1((1, 2, 3)); });
t("delete member auto", auto sub () { Holder h(); h.a = (1, 2); delete h.a[0]; return h.a; });
t("delete member declared", auto sub () { Holder h(); h.i = (1, 2); delete h.i[0]; return h.i; });
t("delete global auto", auto sub () { ga = (1, 2); delete ga[0]; return ga; });
t("delete global declared", auto sub () { gi = (1, 2); delete gi[0]; return gi; });
t("delete closure local", auto sub () {
    auto l = (1, 2);
    code c = sub () { delete l[0]; };
    c();
    return l;
});
t("delete closure declared local", auto sub () {
    list<int> l = (1, 2);
    code c = sub () { delete l[0]; };
    c();
    return l;
});
# after a delete, the readers see the list<*int>
t("select after delete", auto sub () { auto l = (10, 11, 12); delete l[1]; return select l, True; });
t("select after delete in a function", auto sub () { auto l = (10, 11, 12); delete l[1]; return sel_all(l); });
t("select int after delete", auto sub () { auto l = (10, 11, 12); delete l[1]; return select l, $1 > 10; });
t("map after delete", auto sub () { auto l = (10, 11, 12); delete l[1]; return map $1, l; });
t("range after delete", auto sub () { auto l = (10, 11, 12); delete l[1]; return l[0..2]; });
t("foreach after delete", auto sub () {
    auto l = (10, 11, 12);
    delete l[1];
    list<auto> r = ();
    foreach auto v in (l) {
        push r, v;
    }
    return r;
});
t("foreach int after delete", auto sub () {
    auto l = (10, 11, 12);
    delete l[1];
    list<auto> r = ();
    foreach int v in (l) {
        push r, v;
    }
    return r;
});
t("fold after delete", auto sub () { auto l = (10, 11, 12); delete l[1]; return foldl $1 + $2, l; });
t("index after delete", auto sub () { auto l = (10, 11, 12); delete l[1]; return l[1]; });
t("remove past the end", auto sub () { list<int> l = (10,); auto v = remove l[3]; return (v, l); });
t("delete past the end", auto sub () { list<int> l = (10,); delete l[3]; return l; });
t("nested delete past the end", auto sub () { list<list<int>> l = ((1,),); delete l[3][0]; return l; });
# a range and a list of indices remove the elements from the list, as splice and extract do
t("delete range", auto sub () { list<int> l = (10, 11, 12, 13); delete l[1..2]; return l; });
t("remove range", auto sub () { list<int> l = (10, 11, 12, 13); auto v = remove l[1..2]; return (v, l); });
t("delete indices", auto sub () { list<int> l = (10, 11, 12, 13); delete l[0, 2]; return l; });
t("remove indices", auto sub () { list<int> l = (10, 11, 12, 13); auto v = remove l[0, 2]; return (v, l); });
t("splice", auto sub () { list<int> l = (10, 11, 12); splice l, 1, 1; return l; });
t("extract", auto sub () { list<int> l = (10, 11, 12); auto v = extract l, 1, 1; return (v, l); });
# lists whose element type takes NOTHING keep their holes
t("delete mixed auto", auto sub () { auto l = (1, "a"); delete l[0]; return l; });

# the readers of a list with a hole, whose element type takes NOTHING
t("range or-nothing", auto sub () { list<*int> l = (10, 11, 12); delete l[1]; return l[0..2]; });
t("map or-nothing", auto sub () { list<*int> l = (10, 11, 12); delete l[1]; return map $1, l; });
t("select or-nothing", auto sub () { list<*int> l = (10, 11, 12); delete l[1]; return select l, True; });
t("select or-nothing holes", auto sub () { list<*int> l = (10, 11, 12); delete l[1]; return select l, !exists $1; });
t("foreach or-nothing", auto sub () {
    list<*int> l = (10, 11, 12);
    delete l[1];
    list<auto> r = ();
    foreach *int v in (l) {
        push r, v;
    }
    return r;
});
# select over typed lists without holes
t("select int", auto sub () { return select_ints((10, 11, 12)); });
t("select int again", auto sub () { return select_ints((9, 13)); });
t("select float", auto sub () { list<float> l = (1.5, 2.5, 3.5); return select l, $1 > 2.0; });
t("select string", auto sub () { list<string> l = ("a", "bb", "c"); return select l, $1.size() == 1; });
t("select all", auto sub () { list<int> l = (1, 2, 3); return select l, True; });
# the cases of qore#6437: a gap fails, a delete of an element of a declared list<int> fails, and a delete of an element
# of a list held by auto gives a list<*int>, which select gives as it is (above)
t("qore#6437 gap", auto sub () { list<int> l = (10,); l[3] = 13; return select l, True; });
t("qore#6437 delete", auto sub () { list<int> d = (10, 11, 12); delete d[1]; return select d, True; });
