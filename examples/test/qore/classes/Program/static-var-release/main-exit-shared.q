# -*- mode: qore; indent-tabs-mode: nil -*-
# main-exit-shared.q - script of static-var-release.qtest: the static variables of the main Program are released at
# teardown while a Program created by a destructor shares classes whose variables have already been released

# Copyright 2026 Qore Technologies, s.r.o.

%modern

#! a class whose static variables are released before Holder's (the classes are cleared in name order)
public class Alpha {
    public {
        static int v = 5;
        static hash<auto> h = {"a": 1};
    }
}

#! creates and deletes a Program in its destructor that shares the classes of this Program, then reads Alpha's
#! variables, which the deleted Program shared
public class Guard {
    destructor() {
        {
            Program p(PO_NEW_STYLE);
            p.parse("int sub get() { return Holder::value(); }", "guard");
            printf("guard Program result: %d\n", p.callFunction("get"));
        }
        printf("guard read: %y %y\n", Alpha::v, Alpha::h);
    }
}

#! holds the guard in a static variable
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

#! reads the variables of Alpha and Holder in its destructor, after Holder's have been released
class Reader {
    destructor() {
        printf("reader read: %y %y\n", Alpha::v, Holder::before);
    }
}

#! holds the reader in a static variable released after Holder's
class Zulu {
    public {
        static Reader reader = new Reader();
    }
}

printf("main done: %d\n", Holder::value());
