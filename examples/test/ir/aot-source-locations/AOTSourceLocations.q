#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# prints the lines of the calls that member initializers and default arguments make, and of an exception in a member
# initializer; run by aot-source-locations.qtest in every execution mode and compiled ahead of time, where these
# expressions are AST trees restored from the image

%modern

#! prints the lines of the frame of its caller's call
sub report(string what) {
    hash<auto> f = get_thread_call_stack()[2];
    printf("%s: %d-%d\n", what, f.line, f.endline);
}

int sub zero() {
    return 0;
}

int sub fdef() {
    report("function call default");
    return 1;
}

class K {
    static int sdef() {
        report("static method call default");
        return 2;
    }

    string mdef() {
        report("method call on a constant default");
        return "x";
    }
}

const KO = new K();

int sub exprdef() {
    report("expression default");
    return 3;
}

int sub memb() {
    report("member initializer");
    return 4;
}

int sub withDefaults(int a = fdef(), int b = K::sdef(), int c = exprdef() + 1, string d = KO.mdef()) {
    return a + b + c + d.size();
}

class M {
    private {
        int v = memb();
    }
}

class E {
    private {
        int v = 1 / zero();
    }
}

int r = withDefaults();
M m();
try {
    E e();
} catch (hash<ExceptionInfo> ex) {
    printf("member initializer exception: %s %d-%d\n", ex.err, ex.line, ex.endline);
}
printf("r=%d\n", r);
