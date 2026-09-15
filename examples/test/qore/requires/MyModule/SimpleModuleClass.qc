# -*- mode: qore; indent-tabs-mode: nil -*-
# Qorus SimpleModuleClass class definition

public namespace EXAMPLE_C {
    public class SomeClass {
        public string cfunc(string str) {
            return str + "EXAMPLE_C::SomeClass::cfunc\n";
        }
    }

    public string sub func(string str) {
        return str + "EXAMPLE_C::func\n";
    }
}
