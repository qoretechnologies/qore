#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

%modern

# method calls whose arguments can have side effects: the target is checked before and after the arguments; in native
# code, the check after them tests only the deleted status of an object that passed the check before them
# (issue #6646)

class C {
    int m(int a) {
        return a + 1;
    }
}

class D inherits C {
    int m(int a) {
        return a + 100;
    }
}

int sub del(C o) {
    delete o;
    return 1;
}

int sub keep(C o) {
    return 2;
}

# the object of the call is deleted by its argument
int sub deleted_by_arg(C o) {
    return o.m(del(o));
}

# the object of the call is deleted by its argument through another variable
int sub deleted_by_arg_other(C o) {
    C other = o;
    return o.m(del(other));
}

# the argument does not delete the object
int sub kept_by_arg(C o) {
    return o.m(keep(o));
}

# the object is deleted in the third iteration of a loop
int sub deleted_in_loop(C o) {
    int t = 0;
    for (int i = 0; i < 4; ++i) {
        t += o.m(i == 2 ? del(o) : keep(o));
        printf("  iteration %d: %d\n", i, t);
    }
    return t;
}

# the implicit argument of map, which the lowering treats as having side effects
list<int> sub map_implicit(C o, list<int> l) {
    return map o.m($1), l;
}

# the element whose call deletes the object raises the error
list<int> sub map_deleted(C o, list<int> l) {
    return map o.m($1 == 3 ? del(o) : $1), l;
}

sub run(string label, code c) {
    try {
        printf("%s: %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

list<int> l = (1, 2, 3, 4);
foreach string cls in ("C", "D") {
    code make = cls == "C" ? C sub () { return new C(); } : C sub () { return new D(); };
    printf("class %s\n", cls);
    run("deleted by argument", auto sub () { return deleted_by_arg(make()); });
    run("deleted by argument through another variable", auto sub () { return deleted_by_arg_other(make()); });
    run("kept by argument", auto sub () { return kept_by_arg(make()); });
    run("deleted in loop", auto sub () { return deleted_in_loop(make()); });
    run("map with implicit argument", auto sub () { return map_implicit(make(), l); });
    run("map deleted by argument", auto sub () { return map_deleted(make(), l); });
}
