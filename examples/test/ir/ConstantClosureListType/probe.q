#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# the probe of ConstantClosureListType.qtest: lists of closures nested in constant expressions have the type of their
# closures, as the same expressions outside a constant expression and at the top level of a constant (qore#5972)

%modern

const C1 = {"l": (sub () {},)};
const C2 = ({"a": (sub () {},)},);
const C3 = (sub () {},);
const C4 = {"h": {"l": (sub () {},)}};
const C5 = {"l": (int sub (int x) { return x; },)};
const C6 = {"l": (sub () {},), "n": 1};
const C7 = {"h": ({"l": (sub () {},)},)};

hashdecl ClosureListHolder {
    list<code> l;
}

const C8 = <ClosureListHolder>{"l": (sub () {},)};
const hash<string, list<code>> C9 = {"l": (sub () {},)};

sub show(string label, auto v) {
    printf("%s: %s\n", label, v.fullType());
}

sub f() {
    show("f hash of list", {"l": (sub () {},)});
    show("f list of hash of list", ({"a": (sub () {},)},));
    show("f list", (sub () {},));
    show("f hash of hash of list", {"h": {"l": (sub () {},)}});
    show("f hash of list with signature", {"l": (int sub (int x) { return x; },)});
    show("f hash of list and int", {"l": (sub () {},), "n": 1});
    show("f hash of list of hash of list", {"h": ({"l": (sub () {},)},)});
    show("f hashdecl", <ClosureListHolder>{"l": (sub () {},)});
    hash<string, list<code>> c9 = {"l": (sub () {},)};
    show("f typed", c9);
}

show("const hash of list", C1);
show("const list of hash of list", C2);
show("const list", C3);
show("const hash of hash of list", C4);
show("const hash of list with signature", C5);
show("const hash of list and int", C6);
show("const hash of list of hash of list", C7);
show("const hashdecl", C8);
show("const typed", C9);
f();
# the closures run
printf("call: %y\n", C5.l[0](2));
