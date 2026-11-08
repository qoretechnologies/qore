# -*- mode: qore; indent-tabs-mode: nil -*-
# main-exit-stored-child.q - script of static-var-release.qtest: a child Program is kept in a static variable of a
# public class that it shares; the values, and the child, are released when the main Program is cleared at exit

# Copyright 2026 Qore Technologies, s.r.o.

%modern

#! reports its release
class Marker {
    destructor() {
        printf("marker released\n");
    }
}

#! holds the child Program, which shares this class, and a marker
public class D {
    public {
        static *Program p;
        static Marker m = new Marker();
    }
}

D::p = new Program(PO_NEW_STYLE);
D::p.parse("int sub get() { return 42; }", "child");
printf("main done %d\n", D::p.callFunction("get"));
