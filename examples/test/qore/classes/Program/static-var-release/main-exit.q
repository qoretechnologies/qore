# -*- mode: qore; indent-tabs-mode: nil -*-
# main-exit.q - script of static-var-release.qtest: a static variable of the main Program is released at teardown

# Copyright 2026 Qore Technologies, s.r.o.

%modern

#! creates a Program in its destructor that shares the classes of this Program and runs code using them
public class Guard {
    destructor() {
        Program p(PO_NEW_STYLE);
        p.parse("int sub get() { return Holder::value(); }", "guard");
        printf("guard Program result: %d\n", p.callFunction("get"));
    }
}

#! holds the guard in a static variable; the other static variables are released around it
public class Holder {
    public {
        static int before = 1;
        static Guard guard = new Guard();
        static hash<auto> after = {"a": 1};
    }

    static int value() {
        return 42;
    }
}

printf("main done: %d\n", Holder::value());
