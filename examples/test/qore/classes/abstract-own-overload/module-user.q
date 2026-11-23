#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# calls the methods of the classes of the AbstractOwnOverload module and of classes of its own that inherit them; the
# test runs it with the module's source and with a qcc -m .qmod and compares the output with module-user.expected

%modern
%disable-warning non-existent-method-call
%requires AbstractOwnOverload

class SBool { string m(bool b) { return "SBool::m(bool)"; } }

# subclasses of the module's classes
class U3 inherits AOO::C3 {}
class U3b inherits AOO::C3, AOO::I3, SBool {}
class U3c inherits AOO::C3, AOO::I3 { string m(bool b) { return "U3c::m(bool)"; } }
class U3d inherits AOO::I3, AOO::C3 {}
class U18 inherits AOO::C18 { string m(bool b) { return "U18::m(bool)"; } }
class U12 inherits AOO::C12 { string m(bool b) { return "U12::m(bool)"; } }
# the module's classes as parent classes of a class of the program with its own overload
class UC inherits AOO::Sint, AOO::I1, AOO::I2 { string m(string s) { return "UC::m(string)"; } }
class UC5 inherits AOO::I2, AOO::I1, AOO::Sstr { string m(int i) { return "UC5::m(int)"; } }

string sub call(object o, auto arg) {
    try {
        return exists arg ? o.m(arg) : o.m();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

foreach string cn in ("AOO::C3", "AOO::C4", "AOO::C5", "AOO::C13", "AOO::P1", "AOO::Q1", "AOO::C12", "AOO::C18",
        "AOO::K6", "U3", "U3b", "U3c", "U3d", "U18", "U12", "UC", "UC5") {
    object o;
    try {
        o = create_object(cn);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", cn, ex.err);
        continue;
    }
    printf("%s: %s\n", cn, (map sprintf("m(%s): %s", $1.type(), call(o, $1)), (1, "a", True)).join("; "));
}
object o = create_object("AOO::Q1");
printf("AOO::Q1 via: %s %s\n", o.via(1), o.via("a"));
