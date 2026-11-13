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
}

t();
# top-level code runs natively with --exec-mode=jit
show("top cast", (cast<list<code>>((sub () {},)),));
show("top cast in hash", {"l": (cast<list<code>>((sub () {},)),)});
