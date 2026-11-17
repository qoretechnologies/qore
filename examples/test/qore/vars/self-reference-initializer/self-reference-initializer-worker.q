#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# Uses of a variable in its own initialization expression that remain valid (qore#526): in a closure created there,
# which reads the variable when it is called, and the right-hand side of a list declaration, which is parsed before
# its variables are declared.  The cases run three times, so that tiered execution compiles them, and print in the
# last run.

%modern
# the list declaration case declares a variable of the outer block again
%disable-warning duplicate-local-vars

class C {
    public {
        int a = 1;
        int b = a + 1;
        static int s = 1;
        static int t = s + 1;
    }
}

#! true in the last run, which prints the results
our bool print_results;

#! a global whose initializer calls a function that reads it: the function body is not the initializer
our auto g = get_g();

auto sub get_g() {
    return g ?? 7;
}

#! a global declared in a function keeps its value between the calls
int sub counter() {
    our int n = n + 1;
    return n;
}

#! runs one case and prints its outcome
sub check(string label, code op) {
    string outcome;
    try {
        outcome = sprintf("%y", op());
    } catch (hash<ExceptionInfo> ex) {
        outcome = ex.err;
    }
    if (print_results) {
        printf("%s: %s\n", label, outcome);
    }
}

sub run_cases() {
    check("closure recursion", auto sub () {
        code fact = int sub (int n) { return n <= 1 ? 1 : n * fact(n - 1); };
        return fact(5);
    });
    check("nested closure", auto sub () {
        code c = auto sub () { code d = auto sub () { return c; }; return d(); };
        return c() == c;
    });
    check("closure exists", auto sub () {
        code c = auto sub () { return exists c; };
        return c();
    });
    check("list declaration outer", auto sub () {
        int y = 5;
        {
            (int x, int y) = (y, 1);
            return (x, y);
        }
    });
    check("assignment of declared variable", auto sub () { int a = 1; a = a + 1; return a; });
    check("member", auto sub () { C c(); return c.b; });
    check("static", auto sub () { return C::t; });
    check("foreach declared before", auto sub () {
        list<auto> l = ();
        int i = 1;
        foreach i in ((i, 2)) {
            l += i;
        }
        return l;
    });
    check("global through function", auto sub () { return g; });
    # two calls in each of the three runs: the last run gives the fifth and the sixth values
    check("global declared in a function", auto sub () { return (counter(), counter()); });
}

for (int i = 0; i < 3; ++i) {
    print_results = (i == 2);
    run_cases();
}
