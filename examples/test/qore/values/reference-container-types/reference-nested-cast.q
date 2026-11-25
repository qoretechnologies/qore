#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# a cast to a nested container type keeps a reference element only if the element type can hold one (qore#7377)

%modern

our int g_i = 7;

sub show(string label, code c) {
    try {
        printf("%s: %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

list<auto> inner = (\g_i,);
list<list<auto>> nested_list = (inner,);
hash<string, list<auto>> hash_of_list = {"a": inner};
list<hash<auto>> list_of_hash = ({"k": \g_i},);
hash<string, hash<auto>> hash_of_hash = {"a": {"k": \g_i}};

show("source before the casts", auto sub () { return nested_list[0].fullType() + " " + nested_list[0][0].type(); });

# element types that cannot hold a reference
show("top level", auto sub () { list<int> l = cast<list<int>>(inner); return l.fullType(); });
show("list of list of int", auto sub () { list<list<int>> l = cast<list<list<int>>>(nested_list); return l.fullType(); });
show("list of list of string", auto sub () { list<list<string>> l = cast<list<list<string>>>(nested_list); return l.fullType(); });
show("list of list of softint", auto sub () { list<list<softint>> l = cast<list<list<softint>>>(nested_list); return l.fullType(); });
show("hash of list of int", auto sub () { hash<string, list<int>> h = cast<hash<string, list<int>>>(hash_of_list); return h.fullType(); });
show("list of hash of int", auto sub () { list<hash<string, int>> l = cast<list<hash<string, int>>>(list_of_hash); return l.fullType(); });
show("hash of hash of int", auto sub () { hash<string, hash<string, int>> h = cast<hash<string, hash<string, int>>>(hash_of_hash); return h.fullType(); });
show("list of list of reference to string", auto sub () {
    list<list<reference<string>>> l = cast<list<list<reference<string>>>>(nested_list);
    return l.fullType();
});
show("hash of list of reference to string", auto sub () {
    hash<string, list<reference<string>>> h = cast<hash<string, list<reference<string>>>>(hash_of_list);
    return h.fullType();
});

# a variable of a wider type can hold a value of the element type
auto wide_int = 8;
list<auto> wide_int_inner = (\wide_int,);
list<list<auto>> wide_ints = (wide_int_inner,);
show("reference to int of an auto variable holding an int", auto sub () {
    list<list<reference<int>>> l = cast<list<list<reference<int>>>>(wide_ints);
    l[0][0] = 9;
    return (l.fullType(), wide_int);
});

# the value given to a failed cast is unchanged
show("source after a failed cast", auto sub () { return nested_list[0].fullType() + " " + nested_list[0][0].type(); });

# element types that can hold a reference keep it
show("list of list of reference", auto sub () {
    list<list<reference<int>>> l = cast<list<list<reference<int>>>>(nested_list);
    l[0][0] = 100;
    return (l.fullType(), g_i);
});
g_i = 7;
show("list of list of optional reference", auto sub () {
    list<list<*reference<int>>> l = cast<list<list<*reference<int>>>>(nested_list);
    l[0][0] = 101;
    return (l.fullType(), g_i);
});
g_i = 7;
show("list of list of auto", auto sub () {
    list<list<auto>> l = cast<list<list<auto>>>(nested_list);
    l[0][0] = 102;
    return (l.fullType(), g_i);
});
g_i = 7;

our string g_s = "5";
list<auto> string_ref_inner = (\g_s,);
list<list<auto>> string_ref = (string_ref_inner,);
show("list of list of reference to softint", auto sub () {
    list<list<reference<softint>>> l = cast<list<list<reference<softint>>>>(string_ref);
    return (l.fullType(), g_s.type(), g_s);
});

# values that are not references are still converted
list<list<auto>> strings = (("3",),);
show("conversion of a string", auto sub () {
    list<list<softint>> l = cast<list<list<softint>>>(strings);
    int v = l[0][0];
    return v + 1;
});
