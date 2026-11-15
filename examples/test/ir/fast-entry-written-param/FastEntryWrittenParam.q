# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern

# A callee that writes a parameter of an optional object type, whose argument is a value taken out of an lvalue: the
# parameter is written, so a fast entry cannot treat it as unused (issue #6148)
# the release order of the arguments is that of issue #5958; only the calls and the written values are printed
class T {
    public { string n; }
    constructor(string n) { self.n = n; }
}
sub obj_clear(*T k, string tag) {
    k = NOTHING;
    printf("after write %s: %y\n", tag, k);
}
sub obj_set(*T k, string tag) {
    k = new T(tag + "-new");
    printf("after write %s: %s\n", tag, k.n);
}
# the shape of issue #6148: one parameter, called from a function with no parameters
sub obj_clear1(*T k) {
    k = NOTHING;
    printf("after write: %y\n", k);
}
sub remove_local() {
    *T v = new T("lv-rm");
    obj_clear1(remove v);
}
sub calls(int i) {
    {
        *T v = new T("rm" + i);
        obj_clear(remove v, "rm" + i);
    }
    {
        list<auto> l = (new T("sh" + i),);
        obj_clear(shift l, "sh" + i);
    }
    {
        *T v = new T("set" + i);
        obj_set(remove v, "set" + i);
    }
}
# twice for the first calls and the cached call sites of every execution mode
for (int i = 0; i < 2; ++i) {
    remove_local();
    calls(i);
}
