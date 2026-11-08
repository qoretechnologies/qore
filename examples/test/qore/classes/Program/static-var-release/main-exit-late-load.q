# -*- mode: qore; indent-tabs-mode: nil -*-
# main-exit-late-load.q - script of static-var-release.qtest: a destructor run while the static variables of the main
# Program are released loads a module whose classes are added to the main Program's class lists

# Copyright 2026 Qore Technologies, s.r.o.

%modern

#! loads the module in its destructor and uses one of its classes
public class Guard {
    destructor() {
        load_module("StaticVarReleaseLate");
        object o = create_object("LateHolder");
        printf("loaded at teardown: %s\n", o.className());
    }
}

#! holds the guard in a static variable
public class Holder {
    public {
        static Guard guard = new Guard();
    }
}

printf("main done\n");
