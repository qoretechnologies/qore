#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# instantiates the classes of the AbstractInternalEdge module and classes of its own that inherit them, and calls
# their methods from outside their hierarchy; the test runs it with the module's source and with a qcc -m .qmod and
# compares the output with module-user.expected

%modern
%requires AbstractInternalEdge

# abstract: the module's K2 inherits Secret with private:internal
class UJ2 inherits AIE::K2, AIE::Iface {}
class UJ3 inherits AIE::K3, AIE::Iface {}
# implemented
class UP1 inherits AIE::K2, AIE::Secret, AIE::Iface {}
class UP6 inherits AIE::P6 {}

foreach string cn in ("AIE::J2", "AIE::J1", "AIE::J3", "AIE::P1", "AIE::P6", "UJ2", "UJ3", "UP1", "UP6") {
    object o;
    try {
        o = create_object(cn);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", cn, ex.err);
        continue;
    }
    auto a = o;
    try {
        printf("%s: %y\n", cn, a.m());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", cn, ex.err);
    }
    try {
        printf("%s call_object_method: %y\n", cn, call_object_method(o, "m"));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s call_object_method: %s\n", cn, ex.err);
    }
}
