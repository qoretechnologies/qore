#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

%modern
%allow-weak-references

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

# a class without subclasses: a call on a deleted object that the check after the arguments did not catch would raise
# the error of the call, which names the method's signature, instead of that of the check
class E {
    int n(int a) {
        return a + 1;
    }
}

int sub dele(E o) {
    delete o;
    return 1;
}

int sub e_deleted_by_arg(E o) {
    return o.n(dele(o));
}

int sub e_deleted_by_arg_other(E o) {
    E other = o;
    return o.n(dele(other));
}

int sub e_removed_deleted_by_arg(E o) {
    E t = o.copy();
    return t.n(dele(remove t));
}

int sub e_weak_deleted_by_arg(E o) {
    E w := o;
    return w.n(dele(o));
}

list<int> sub e_map_deleted(E o, list<int> l) {
    return map o.n($1 == 3 ? dele(o) : $1), l;
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

# the target variable is reassigned by the argument: the check stays on the original object, which only the
# variable held
int sub reassigned_by_arg(C o) {
    C t = o.copy();
    return t.m((t = new C()).m(0));
}

# the target variable is reassigned by the argument after the argument deletes the original object
int sub reassigned_deleted_by_arg(C o) {
    C t = o.copy();
    return t.m(del(t) + (t = new C()).m(0));
}

# the target variable is removed by the argument: the check stays on the original object
int sub removed_by_arg(C o) {
    C t = o.copy();
    return t.m(keep(remove t));
}

# the target variable is removed by the argument, which deletes the object
int sub removed_deleted_by_arg(C o) {
    C t = o.copy();
    return t.m(del(remove t));
}

# a weak reference to the object as the target, deleted by the argument through the strong reference
int sub weak_deleted_by_arg(C o) {
    C w := o;
    return w.m(del(o));
}

# a weak reference to the object as the target, not deleted
int sub weak_kept_by_arg(C o) {
    C w := o;
    return w.m(keep(o));
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

C sub make(string cls) {
    return cls == "C" ? new C() : new D();
}

# the calls are made directly in the top-level code, which native code runs: a call through a closure can take a path
# that raises the same error in the call itself
sub error(string label, hash<ExceptionInfo> ex) {
    printf("%s: %s: %s\n", label, ex.err, ex.desc);
}

list<int> l = (1, 2, 3, 4);
foreach string cls in ("C", "D") {
    printf("class %s\n", cls);
    try {
        printf("deleted by argument: %y\n", deleted_by_arg(make(cls)));
    } catch (hash<ExceptionInfo> ex) {
        error("deleted by argument", ex);
    }
    try {
        printf("deleted by argument through another variable: %y\n", deleted_by_arg_other(make(cls)));
    } catch (hash<ExceptionInfo> ex) {
        error("deleted by argument through another variable", ex);
    }
    try {
        printf("reassigned by argument: %y\n", reassigned_by_arg(make(cls)));
    } catch (hash<ExceptionInfo> ex) {
        error("reassigned by argument", ex);
    }
    try {
        printf("reassigned by argument after deleting: %y\n", reassigned_deleted_by_arg(make(cls)));
    } catch (hash<ExceptionInfo> ex) {
        error("reassigned by argument after deleting", ex);
    }
    try {
        printf("removed by argument: %y\n", removed_by_arg(make(cls)));
    } catch (hash<ExceptionInfo> ex) {
        error("removed by argument", ex);
    }
    try {
        printf("removed and deleted by argument: %y\n", removed_deleted_by_arg(make(cls)));
    } catch (hash<ExceptionInfo> ex) {
        error("removed and deleted by argument", ex);
    }
    try {
        printf("weak reference deleted by argument: %y\n", weak_deleted_by_arg(make(cls)));
    } catch (hash<ExceptionInfo> ex) {
        error("weak reference deleted by argument", ex);
    }
    try {
        printf("weak reference kept by argument: %y\n", weak_kept_by_arg(make(cls)));
    } catch (hash<ExceptionInfo> ex) {
        error("weak reference kept by argument", ex);
    }
    try {
        printf("kept by argument: %y\n", kept_by_arg(make(cls)));
    } catch (hash<ExceptionInfo> ex) {
        error("kept by argument", ex);
    }
    try {
        printf("deleted in loop: %y\n", deleted_in_loop(make(cls)));
    } catch (hash<ExceptionInfo> ex) {
        error("deleted in loop", ex);
    }
    try {
        printf("map with implicit argument: %y\n", map_implicit(make(cls), l));
    } catch (hash<ExceptionInfo> ex) {
        error("map with implicit argument", ex);
    }
    try {
        printf("map deleted by argument: %y\n", map_deleted(make(cls), l));
    } catch (hash<ExceptionInfo> ex) {
        error("map deleted by argument", ex);
    }
}
printf("class E\n");
try {
    printf("deleted by argument: %y\n", e_deleted_by_arg(new E()));
} catch (hash<ExceptionInfo> ex) {
    error("deleted by argument", ex);
}
try {
    printf("deleted by argument through another variable: %y\n", e_deleted_by_arg_other(new E()));
} catch (hash<ExceptionInfo> ex) {
    error("deleted by argument through another variable", ex);
}
try {
    printf("removed and deleted by argument: %y\n", e_removed_deleted_by_arg(new E()));
} catch (hash<ExceptionInfo> ex) {
    error("removed and deleted by argument", ex);
}
try {
    printf("weak reference deleted by argument: %y\n", e_weak_deleted_by_arg(new E()));
} catch (hash<ExceptionInfo> ex) {
    error("weak reference deleted by argument", ex);
}
try {
    printf("map deleted by argument: %y\n", e_map_deleted(new E(), l));
} catch (hash<ExceptionInfo> ex) {
    error("map deleted by argument", ex);
}
