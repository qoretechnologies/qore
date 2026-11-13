#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# the probe of ListLiteralValueType.qtest: the types of list literals whose values are more specific than the
# literal's parse type (qore#5970)

%modern

hashdecl ListLiteralA {
    int a = 1;
}

hashdecl ListLiteralB {
    int b = 2;
}

class ListLiteralC {
}

sub show(string label, auto v) {
    printf("%s: %s\n", label, v.fullType());
}

# the result of arithmetic on or-nothing operands has an or-nothing parse type (from supah's #5663 review)
list<auto> sub sum_list(*int x) {
    return (x + x,);
}

list<auto> sub concat_list(*string s) {
    return (s + s,);
}

list<auto> sub var_list(*int x) {
    return (x,);
}

# literals that mix a type parameter with another type (qore#6000)
auto sub tparam_list<T>(list<T> t) {
    return map ($1, "x"), t;
}

auto sub tparam_list_nothing<T>(list<T> t) {
    return map ($1, 1), t;
}

auto sub tparam_hash_list<T>(list<T> t) {
    return map {"k" + $#: ($1, "x")}, t;
}

auto sub tparam_hash_hash<T>(list<T> t) {
    return map {"k" + $#: {"a": $1, "b": "x"}}, t;
}

class ListLiteralBox<T> {
    public {
        list<T> values;
    }

    constructor(list<T> values) {
        self.values = values;
    }

    auto gm<U>(list<U> u) {
        return map ($1, values[0]), u;
    }
}

# a constant whose list MakeList rebuilds with the constant's exact type
const ListLiteralTL = (1, 2);
const list<*int> ListLiteralTypedTL = (1, 2);

sub showv(string label, auto v) {
    printf("%s: %y %s\n", label, v, v.fullType());
}

sub t() {
    # the cast gives the inner list the type of its closure; the outer literal's parse type is list<list<code>>
    show("cast", (cast<list<code>>((sub () {},)),));
    show("cast nested", ((cast<list<code>>((sub () {},)),),));
    show("cast two", (cast<list<code>>((sub () {},)), cast<list<code>>((sub () {},))));
    show("cast in hash", {"l": (cast<list<code>>((sub () {},)),)});
    show("cast hash value", {"l": cast<list<code>>((sub () {},))});
    # values of no common type, and values whose types the parse types already give
    show("hashdecls", (<ListLiteralA>{}, <ListLiteralB>{}));
    show("int and string", (1, "x"));
    list l = (1, 2);
    list<int> li = (1, 2);
    show("list and list<int>", (l, li));
    *int n;
    show("NOTHING and int", (n, 1));
    show("objects", (new ListLiteralC(), new ListLiteralC()));
    show("list<int> twice", (li, li));
    hash<string, int> hi = {"a": 1};
    show("hash<string, int> and hash", (hi, {"b": 2}));
    show("empty", ());
    showv("or-nothing sum NOTHING", sum_list());
    showv("or-nothing sum int", sum_list(2));
    showv("or-nothing concat NOTHING", concat_list());
    showv("or-nothing concat string", concat_list("a"));
    showv("or-nothing var NOTHING", var_list());
    showv("type parameter and string", tparam_list<int>((1, 2)));
    showv("type parameter and int", tparam_list_nothing<int>((1, 2)));
    showv("type parameter and string in a hash", tparam_hash_list<int>((1, 2)));
    showv("type parameter and string in a hash of a hash", tparam_hash_hash<int>((1, 2)));
    ListLiteralBox<string> box(("v",));
    showv("method type parameter and class type parameter", box.gm<int>((1, 2)));
    # an exact declared type is kept: a typed constant and a typed local initialized with a literal
    auto tc = ListLiteralTypedTL;
    tc[0] = NOTHING;
    showv("typed constant", tc);
    showv("untyped constant", ListLiteralTL);
    *int a = 1;
    *int b = 2;
    list<*int> tl = (a, b);
    tl[0] = NOTHING;
    showv("typed local", tl);
    list<list<*int>> nl = ((a, b),);
    nl[0][0] = NOTHING;
    showv("nested typed local", nl);
}

t();
# top-level code runs natively with --exec-mode=jit
*int ta = 1;
*int tb = 2;
list<*int> ttl = (ta, tb);
ttl[0] = NOTHING;
showv("top typed local", ttl);
auto ttc = ListLiteralTypedTL;
ttc[0] = NOTHING;
showv("top typed constant", ttc);
show("top cast", (cast<list<code>>((sub () {},)),));
show("top cast in hash", {"l": (cast<list<code>>((sub () {},)),)});
