#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# push and unshift of an lvalue's own value use the value that the operand had when it was evaluated

%modern
%strict-types

list<auto> sub push_list() { list l = (1, 2); push l, l; return l; }
list<auto> sub unshift_list() { list l = (1, 2); unshift l, l; return l; }
list<auto> sub push_list_auto() { list<auto> l = (1, "x"); push l, l; return l; }
list<auto> sub unshift_list_auto() { list<auto> l = (1, "x"); unshift l, l; return l; }
list<auto> sub push_softlist() { softlist<auto> l = (1, 2); push l, l; return l; }
list<auto> sub unshift_softlist() { softlist<auto> l = (1, 2); unshift l, l; return l; }
list<auto> sub push_untyped() { auto l = (1, "x"); push l, l; return l; }
list<auto> sub unshift_untyped() { auto l = (1, "x"); unshift l, l; return l; }
list<auto> sub push_twice() { list l = (1, 2); push l, l; push l, l; return l; }
list<auto> sub unshift_twice() { list l = (1, 2); unshift l, l; unshift l, l; return l; }
list<auto> sub push_empty() { list l = (); push l, l; return l; }
list<auto> sub unshift_empty() { list l = (); unshift l, l; return l; }
list<auto> sub push_member() { hash<auto> h = {"a": (1, "x")}; push h.a, h.a; return h.a; }
list<auto> sub unshift_member() { hash<auto> h = {"a": (1, "x")}; unshift h.a, h.a; return h.a; }
list<auto> sub push_element() { list l = ((1, 2),); push l[0], l[0]; return l; }
list<auto> sub unshift_element() { list l = ((1, 2),); unshift l[0], l[0]; return l; }
# a fresh local list that a loop pushes into: the push of the list itself copies it
list<auto> sub push_loop() { list l = (); for (int i = 0; i < 3; ++i) { push l, i; } push l, l; return l; }
list<auto> sub push_self_loop() { list l = (1,); for (int i = 0; i < 2; ++i) { push l, l; } return l; }
list<auto> sub push_fresh_self() { list<auto> l = (); push l, 1; push l, l; return l; }
list<auto> sub push_alias() { list l = (1, 2); list a = l; push l, l; unshift a, a; return (l, a); }

our list g = (1, 2);
list<auto> sub push_global() { g = (1, 2); push g, g; return g; }
list<auto> sub unshift_global() { g = (1, 2); unshift g, g; return g; }

class Holder {
    public { list l = (1, 2); }
    list<auto> push() { push l, l; return l; }
    list<auto> unshift() { unshift l, l; return l; }
}

const Expected = {
    "push_list": (1, 2, (1, 2)),
    "unshift_list": ((1, 2), 1, 2),
    "push_list_auto": (1, "x", (1, "x")),
    "unshift_list_auto": ((1, "x"), 1, "x"),
    # the list created from the literal is a list<int>, which does not accept a list
    "push_softlist": "RUNTIME-TYPE-ERROR",
    "unshift_softlist": "RUNTIME-TYPE-ERROR",
    "push_untyped": (1, "x", (1, "x")),
    "unshift_untyped": ((1, "x"), 1, "x"),
    "push_twice": (1, 2, (1, 2), (1, 2, (1, 2))),
    "unshift_twice": ((((1, 2), 1, 2)), (1, 2), 1, 2),
    "push_empty": ((),),
    "unshift_empty": ((),),
    "push_member": (1, "x", (1, "x")),
    "unshift_member": ((1, "x"), 1, "x"),
    "push_element": ((1, 2, (1, 2)),),
    "unshift_element": (((1, 2), 1, 2),),
    "push_loop": (0, 1, 2, (0, 1, 2)),
    "push_self_loop": (1, (1,), (1, (1,))),
    "push_fresh_self": (1, (1,)),
    "push_alias": ((1, 2, (1, 2)), ((1, 2), 1, 2)),
    "push_global": (1, 2, (1, 2)),
    "unshift_global": ((1, 2), 1, 2),
};

foreach hash<auto> i in (Expected.pairIterator()) {
    auto v;
    try {
        v = call_function(i.key);
    } catch (hash<ExceptionInfo> ex) {
        v = ex.err;
    }
    printf("%s %s: %y\n", v === i.value ? "PASS" : "FAIL", i.key, v);
}
Holder h();
list<auto> v = h.push();
printf("%s Holder::push: %y\n", v === (1, 2, (1, 2)) ? "PASS" : "FAIL", v);
Holder h2();
v = h2.unshift();
printf("%s Holder::unshift: %y\n", v === ((1, 2), 1, 2) ? "PASS" : "FAIL", v);
