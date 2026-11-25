#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# calls the methods of the classes of the AbstractOverloadParents module and of classes of its own that inherit them
# with further abstract parent classes; the test runs it with the module's source and with a qcc -m .qmod and compares
# the output with module-user.expected

%modern
%disable-warning non-existent-method-call
%requires AbstractOverloadParents

class UIfaceBool { abstract string m(bool b); }
class SBool { string m(bool b) { return "SBool::m(bool)"; } }

# subclasses of the module's classes with abstract parent classes of their own
class UX12b inherits AOP::X12, UIfaceBool {}
class UX12s inherits AOP::X12, UIfaceBool, SBool {}
class UX21s inherits SBool, UIfaceBool, AOP::X21 {}
class UC inherits AOP::B, AOP::IfaceStr {}
class UC2 inherits AOP::IfaceStr, AOP::B, AOP::Secret2 {}
class UR inherits AOP::Q, AOP::SInt { string m(string s) { return "UR::m(string)"; } }
class SStr { string m(string s) { return "SStr::m(string)"; } }
# the overload that the module's class implemented is not taken as implemented here: the public declaration of
# another parent class makes it public
class UM2 inherits private AOP::M2, AOP::IfaceInt, SStr {}
class UM2r inherits AOP::IfaceInt, SStr, private AOP::M2 {}

string sub call(object o, auto arg) {
    try {
        return exists arg ? o.m(arg) : o.m();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

foreach string cn in ("AOP::X12", "AOP::X21", "AOP::XPPS12", "AOP::B", "AOP::C", "AOP::C3", "AOP::Q", "AOP::R",
        "AOP::H5", "AOP::M2", "UX12b", "UX12s", "UX21s", "UC", "UC2", "UR", "UM2", "UM2r") {
    object o;
    try {
        o = create_object(cn);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", cn, ex.err);
        continue;
    }
    printf("%s: %s\n", cn, (map sprintf("m(%s): %s", $1.type(), call(o, $1)), (NOTHING, 1, "a", True)).join("; "));
}
object o = create_object("AOP::XPPS12");
printf("AOP::XPPS12 via: %s %s\n", o.via(1), o.via("a"));
