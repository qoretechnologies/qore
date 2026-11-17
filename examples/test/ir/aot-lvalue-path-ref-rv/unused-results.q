#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# lvalue path operations whose value is not used, in closures, whose bodies a qcc executable runs from the IR of its
# image when it has no native code for them (QORE_DISABLE_AOT_NATIVE_CLOSURES, qore#6328); run by
# AOTLValuePathRefRv.qtest, which compares the output with that of the AST interpreter

%modern

string log;

class D {
    private {
        string n;
    }

    constructor(string n) {
        self.n = n;
    }

    destructor() {
        log += "~" + n;
    }
}

hash<auto> h = {"n": 1.5n, "i": 1, "d": new D("d"), "r": new D("r"), "l": (1, 2, 3)};

# an untyped lvalue: an unused post operation is lowered as the post operation
code post_inc = sub () { h.n++; h.i++; };
code post_dec = sub () { h.n--; h.i--; };
code rm = sub () { log += "|rm"; remove h.r; log += "|"; };
code del = sub () { log += "|del"; delete h.d; log += "|"; };
code pop_unused = sub () { pop h.l; };

for (int i = 0; i < 3; ++i) {
    post_inc();
}
post_dec();
rm();
del();
pop_unused();
printf("n=%y i=%y l=%y keys=%y log=%s\n", h.n, h.i, h.l, keys h, log);
