#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# calls closures created in methods after their object was destroyed, often enough to run them on the IR tier, and
# prints the results

%modern

class C {
    public {
        int v = 7;
    }

    #! returns a closure that does not use the object
    code<int()> getConstant() {
        return int sub () { return 1; };
    }

    #! returns a closure that uses a member of the object
    code<int()> getMember() {
        return int sub () { return v; };
    }
}

int sub try_call(code<int()> c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err == "OBJECT-ALREADY-DELETED" ? -1 : -2;
    }
}

int constant = 0;
int member = 0;
# more calls than the default IR threshold, so that the closures also run on the IR tier
for (int i = 0; i < 250; ++i) {
    # the temporary object is destroyed before its closure is called
    code<int()> c = new C().getConstant();
    constant += try_call(c);
    code<int()> m = new C().getMember();
    member += try_call(m);
}
printf("%d %d\n", constant, member);
