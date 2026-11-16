#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (C) 2026 Qore Technologies, s.r.o.

# a call through a call reference held as the value of a hash key (h.r(...)) whose object has been deleted raises
# OBJECT-ALREADY-DELETED before its arguments are evaluated, as the AST interpreter raises it (issue #6225); its
# output is CallReferenceHashValueDeleted.expected in every execution mode and in programs compiled with qcc

%modern

class C {
    int m(int a) {
        return a;
    }

    int n(int a) {
        return a + 10;
    }

    # a call through a hash value in a method of the object's class
    int callInMethod(hash<auto> h) {
        return h.r(side("in method"));
    }

    # a call through a hash member named like a pseudo-method of hashes
    auto callPseudoNameInMethod(hash<auto> h) {
        return h.hasKey(side("pseudo-method name in method"));
    }
}

int sub side(string label) {
    printf("  side effect: %s\n", label);
    return 1;
}

# a call through a hash value in a function
int sub callInFunction() {
    C c1();
    hash<auto> h = {"r": \c1.m()};
    delete c1;
    return h.r(side("in function"));
}

# a call through a hash member named like a pseudo-method of hashes (<hash>::hasKey()), which the member's call
# reference takes precedence over
auto sub callPseudoNameInFunction() {
    C c1();
    hash<auto> h = {"hasKey": \c1.m()};
    delete c1;
    return h.hasKey(side("pseudo-method name in function"));
}

auto sub callPseudoNameThroughParam(hash<auto> h) {
    return h.hasKey(side("pseudo-method name, parameter"));
}

sub check(string name, code c) {
    try {
        printf("%s: %y\n", name, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", name, ex.err, ex.desc);
    }
}

# the cases run more than once, so that the tiered execution mode compiles them
for (int i = 0; i < 3; ++i) {
    printf("round %d\n", i);
    check("method reference, object deleted", int sub () {
        C c1();
        hash<auto> h = {"r": \c1.m()};
        delete c1;
        return h.r(side("deleted"));
    });
    check("method reference, object valid", int sub () {
        C c1();
        hash<auto> h = {"r": \c1.m()};
        return h.r(side("valid"));
    });
    check("reference to another method, object deleted", int sub () {
        C c1();
        hash<auto> h = {"r": \c1.n()};
        delete c1;
        return h.r(side("other method"));
    });
    check("closure", int sub () {
        hash<auto> h = {"r": int sub (int a) { return a * 2; }};
        return h.r(side("closure"));
    });
    check("call in a method, object deleted", int sub () {
        C c1();
        C c2();
        hash<auto> h = {"r": \c1.m()};
        delete c1;
        return c2.callInMethod(h);
    });
    check("call in a function, object deleted", \callInFunction());
    check("pseudo-method name in a function, object deleted", \callPseudoNameInFunction());
    {
        C c1();
        C c2();
        hash<auto> h = {"hasKey": \c1.m()};
        delete c1;
        check("pseudo-method name through a parameter, object deleted",
            auto sub () { return callPseudoNameThroughParam(h); });
        check("pseudo-method name in a method, object deleted", auto sub () { return c2.callPseudoNameInMethod(h); });
        try {
            printf("pseudo-method name in top-level code, object deleted: %y\n",
                h.hasKey(side("pseudo-method name, top level")));
        } catch (hash<ExceptionInfo> ex) {
            printf("pseudo-method name in top-level code, object deleted: %s: %s\n", ex.err, ex.desc);
        }
    }
    check("pseudo-method of a hash without the member", auto sub () {
        hash<auto> h = {"a": 1};
        return h.hasKey(side("pseudo-method"));
    });
}
