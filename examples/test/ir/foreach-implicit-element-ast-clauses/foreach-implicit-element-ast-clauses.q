%modern
%disable-warning unreferenced-variable
# $# in the clauses of context statements and find expressions, which run as AST, in a foreach body: they read the
# foreach index; see foreach-implicit-element-ast-clauses.qtest (qore#6341)
# Copyright (C) 2026 Qore Technologies, s.r.o.
hash h = {"a": (0, 1, 2)};

list r;
foreach auto x in ((1, 2)) {
    context ({"a": (0, 1)}) where (%a == $#) {
        r += %a;
    }
}
printf("context where %y\n", r);

r = ();
foreach auto x in ((1, 2)) {
    context ({"a": (0, 1, 2)}) sortBy (%a == $# ? -1 : %a) {
        r += %a;
    }
}
printf("context sortBy %y\n", r);

r = ();
foreach auto x in ((1, 2)) {
    context ({"a": (0, 1, 2)}) sortDescendingBy (%a == $# ? 10 : %a) {
        r += %a;
    }
}
printf("context sortDescendingBy %y\n", r);

r = ();
foreach auto x in ((1, 2)) {
    context ({"a": ($#, 5)}) {
        r += %a;
    }
}
printf("context data %y\n", r);

r = ();
foreach auto x in ((1, 2)) {
    r += find %a in h where (%a == $#);
}
printf("find where %y\n", r);

r = ();
foreach auto x in ((1, 2)) {
    r += find %a + $# in h where (%a == 0);
}
printf("find exp %y\n", r);

r = ();
foreach auto x in ((1, 2)) {
    r += find %a in ($# ? h : {"a": (7,)}) where (True);
}
printf("find source %y\n", r);

r = ();
foreach auto x in ((1, 2)) {
    context ({"a": (0, 1)}) {
        r += %a + $#;
    }
}
printf("context body %y\n", r);

r = ();
foreach auto x in ((1, 2)) {
    context ({"a": (0, 1)}) where (%a == $#) {
        r += $#;
    }
}
printf("context where+body %y\n", r);

r = ();
foreach auto x in ((1, 2)) {
    context c1 ({"a": (0, 1)}) {
        context c2 ({"b": (0, 1)}) where (%b == $#) {
            r += %b;
        }
    }
}
printf("nested context where %y\n", r);

# in a function, a method, a typed foreach and a foreach in a closure
sub fn() {
    list r;
    foreach int x in ((1, 2)) {
        context ({"a": (0, 1)}) where (%a == $#) {
            r += %a;
        }
        r += find %a in {"a": (5, 6)} where (%a == $# + 5);
    }
    printf("function %y\n", r);
}
fn();

class C {
    list m() {
        list r;
        foreach string x in (("x", "y")) {
            context ({"a": (0, 1)}) sortDescendingBy (%a == $#) {
                r += %a;
            }
        }
        return r;
    }
}
printf("method %y\n", (new C()).m());

code cl = list sub () {
    list r;
    foreach int x in ((1, 2)) {
        r += find %a in {"a": (0, 1)} where (%a == $#);
    }
    return r;
};
printf("closure %y\n", cl());

# a find in the per-element code of operators
list l = (1, 2);
printf("map find %y\n", map (find %a in h where (%a == $#)), l);
printf("select find %y\n", select l, (find %a in h where (%a == $#)) == 1);
printf("hash map find %y\n", map {string($1): (find %a in h where (%a == $#))}, l);
printf("count find %y\n", (count (find %a in h where (%a == $#)) == 1, l));

# no $# in the body or in the clauses
r = ();
foreach auto x in ((1, 2)) {
    context ({"a": (0, 1)}) where (%a == 1) {
        r += %a + x;
    }
}
printf("no index %y\n", r);

# break, continue and return in the context body, and an exception raised in a where clause
r = ();
foreach auto x in ((1, 2, 3)) {
    context ({"a": (0, 1, 2)}) where (%a == $#) {
        if (%a == 1) {
            continue;
        }
        r += %a;
        break;
    }
}
printf("break continue %y\n", r);

list sub ret() {
    list rv;
    foreach auto x in ((1, 2)) {
        context ({"a": (0, 1)}) where (%a == $#) {
            rv += %a;
            if (%a == 1) {
                return rv;
            }
        }
    }
    return rv;
}
printf("return %y\n", ret());

r = ();
foreach auto x in ((1, 2)) {
    try {
        context ({"a": (0, 1)}) where (%a == 1 / $#) {
            r += %a;
        }
    } catch (hash<ExceptionInfo> ex) {
        r += ex.err + $#;
    }
}
printf("exception in where %y\n", r);
