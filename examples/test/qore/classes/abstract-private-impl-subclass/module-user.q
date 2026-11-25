#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# calls the methods of the classes of the AbstractImplSubclass module and of classes of its own that inherit them from
# outside their hierarchy; the test runs it with the module's source and with a qcc -m .qmod and compares the output
# with module-user.expected

%modern
%requires AbstractImplSubclass

# subclasses of the module's classes
class UY3 inherits AIS::X {
    string m(bool b) { return "UY3::m(bool)"; }
    string via(auto a) { return m(a); }
}
class UZ3 inherits AIS::Y3 {}
class UZ0 inherits AIS::Y0 {
    string m(float f) { return "UZ0::m(float)"; }
}
class UYI3 inherits AIS::XI {
    string m(bool b) { return "UYI3::m(bool)"; }
}
class UYc inherits AIS::Xc {
    string m(bool b) { return "UYc::m(bool)"; }
}

foreach string cn in ("AIS::X", "AIS::Y3", "AIS::Y0", "AIS::Z3", "AIS::XI", "AIS::YI3", "AIS::Xc", "AIS::Yc", "UY3",
        "UZ3", "UZ0", "UYI3", "UYc") {
    object o = create_object(cn);
    auto a = o;
    foreach auto arg in ("a", 1) {
        try {
            printf("%s m(%s): %y\n", cn, arg.type(), a.m(arg));
        } catch (hash<ExceptionInfo> ex) {
            printf("%s m(%s): %s\n", cn, arg.type(), ex.err);
        }
        try {
            printf("%s m(%s) call_object_method: %y\n", cn, arg.type(), call_object_method(o, "m", arg));
        } catch (hash<ExceptionInfo> ex) {
            printf("%s m(%s) call_object_method: %s\n", cn, arg.type(), ex.err);
        }
    }
}
foreach string cn in ("AIS::Y3", "AIS::Z3", "UY3", "UZ3") {
    auto o = create_object(cn);
    foreach auto arg in ("a", 1, True) {
        try {
            printf("%s via(%s): %y\n", cn, arg.type(), o.via(arg));
        } catch (hash<ExceptionInfo> ex) {
            printf("%s via(%s): %s\n", cn, arg.type(), ex.err);
        }
    }
}
