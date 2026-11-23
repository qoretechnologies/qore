#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# a class's own concrete variant implements a parent class's abstract variant also when another parent class declares
# another overload of the method that a third parent class implements (qore#7330); abstract-own-overload.qtest
# compares the output of every execution mode and of qcc executables with probe.expected

%modern
%disable-warning non-existent-method-call

class Sint { string m(int i) { return "Sint::m(int)"; } }
class Sstr { string m(string s) { return "Sstr::m(string)"; } }
class Sbool { string m(bool b) { return "Sbool::m(bool)"; } }
class Sintc { string m(int i) const { return "Sintc::m(int) const"; } }
class I1 { abstract string m(int i); }
class I2 { abstract string m(string s); }
class I3 { abstract string m(bool b); }
class I1c { abstract string m(int i) const; }
class I2c { abstract string m(string s) const; }
class I12 { abstract string m(int i); abstract string m(string s); }

# the class's own variant implements the second parent's overload, a sibling the first one's, in each order
class C3 inherits Sint, I1, I2 { string m(string s) { return "C3::m(string)"; } }
class C4 inherits Sint, I2, I1 { string m(string s) { return "C4::m(string)"; } }
class C5 inherits I1, I2, Sstr { string m(int i) { return "C5::m(int)"; } }
class C6 inherits I2, I1, Sstr { string m(int i) { return "C6::m(int)"; } }
class C15 inherits I1, Sint, I2 { string m(string s) { return "C15::m(string)"; } }
class C16 inherits I12, Sint { string m(string s) { return "C16::m(string)"; } }
class C17 inherits I2, Sint, I12 { string m(string s) { return "C17::m(string)"; } }
# both overloads of its own
class C7 inherits I1, I2 { string m(int i) { return "C7::m(int)"; } string m(string s) { return "C7::m(string)"; } }
# three abstract parent classes, two overloads implemented by siblings
class C13 inherits I1, I2, I3, Sint, Sstr { string m(bool b) { return "C13::m(bool)"; } }
class C14 inherits Sstr, I3, Sint, I2, I1 { string m(bool b) { return "C14::m(bool)"; } }
# classes that stay abstract
class C8 inherits I1, I2 { string m(int i) { return "C8::m(int)"; } }
class C9 inherits I2, I1 { string m(int i) { return "C9::m(int)"; } }
class C12 inherits I1, I2, I3, Sint, Sstr {}
class C18 inherits Sint, I1, I2, I3 { string m(string s) { return "C18::m(string)"; } }
# the class's own abstract overload
class C10 inherits I1, Sint { abstract string m(string s); }
class C11 inherits C10 { string m(string s) { return "C11::m(string)"; } }
# the class's own abstract declaration of a parent class's signature stays, a sibling implements it
class A1 inherits Sint, I1, I2 {
    abstract string m(int i);
    string m(string s) { return "A1::m(string)"; }
}
class A2 inherits I2, I1c, Sintc {
    abstract string m(int i) const;
    string m(string s) { return "A2::m(string)"; }
}
class A3 inherits I1, I2 {
    abstract string m(int i);
    string m(string s) { return "A3::m(string)"; }
}
# the class's own abstract overload and own concrete variant (qore#7330)
class A4 inherits I1 {
    abstract string m(string s);
    string m(int i) { return "A4::m(int)"; }
}
class A5 inherits A4 { string m(string s) { return "A5::m(string)"; } }
# subclasses of a class whose own variant implements an abstract variant
class D3 inherits C3 {}
class D3b inherits C3, I3, Sbool {}
class D3c inherits C3, I3 { string m(bool b) { return "D3c::m(bool)"; } }
class D3d inherits I3, C3 {}

# private parent classes: the access of the variants is that of the class without the other overload
class P1 inherits private Sint, I1, I2 { string m(string s) { return "P1::m(string)"; } }
class P1r inherits private Sint, I1 {}
class P2 inherits private:hierarchy Sint, I1, I2 { string m(string s) { return "P2::m(string)"; } }
class P2r inherits private:hierarchy Sint, I1 {}
class P3 inherits private:internal Sint, I1, I2 { string m(string s) { return "P3::m(string)"; } }
class P3r inherits private:internal Sint, I1 {}
class P4 inherits Sint, private I1, private I2 { string m(string s) { return "P4::m(string)"; } }
class P4r inherits Sint, private I1 {}
class P5 inherits Sint, private:internal I1, I2 { string m(string s) { return "P5::m(string)"; } }
# the class's own variants keep their own access
class Q1 inherits Sint, I1, I2 { private string m(string s) { return "Q1::m(string)"; }
    string via(auto a) { return m(a); }
}
class Q1r inherits I2 { private string m(string s) { return "Q1r::m(string)"; }
    string via(auto a) { return m(a); }
}
class Q2 inherits Sint, I1, I2 { private:internal string m(string s) { return "Q2::m(string)"; }
    string via(auto a) { return m(a); }
}
class Q2r inherits I2 { private:internal string m(string s) { return "Q2r::m(string)"; }
    string via(auto a) { return m(a); }
}
class Q3 inherits Sint, I1, I2 { private:hierarchy string m(string s) { return "Q3::m(string)"; }
    string via(auto a) { return m(a); }
}

# const declarations keep their contract
class K1 inherits Sstr, I2, I1, I1c { string m(int i) const { return "K1::m(int) const"; } }
class K2 inherits Sstr, I1c, I12 { string m(int i) const { return "K2::m(int) const"; } }
class K3 inherits Sintc, I1, I1c, I2 { string m(string s) { return "K3::m(string)"; } }
class K4 inherits I2, I1c, Sintc, I1 { string m(string s) { return "K4::m(string)"; } }
class K5 inherits Sint, I1, I2c { string m(string s) const { return "K5::m(string) const"; } }
class K6 inherits Sint, I1c, I2 { string m(string s) { return "K6::m(string)"; } }
class K7 inherits Sint, I2, I1c { string m(string s) { return "K7::m(string)"; } }

string sub call(object o, auto arg) {
    try {
        return exists arg ? o.m(arg) : o.m();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# calls m() from the code of the class itself
string sub selfcall(object o, auto arg) {
    try {
        return o.via(arg);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

foreach string cn in ("C3", "C4", "C5", "C6", "C15", "C16", "C17", "C7", "C13", "C14", "C8", "C9", "C12", "C18",
        "C10", "C11", "A1", "A2", "A3", "A4", "A5", "D3", "D3b", "D3c", "D3d", "P1", "P1r", "P2", "P2r", "P3", "P3r", "P4", "P4r", "P5", "Q1",
        "Q1r", "Q2", "Q2r", "Q3", "K1", "K2", "K3", "K4", "K5", "K6", "K7") {
    object o;
    try {
        o = create_object(cn);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", cn, ex.err);
        continue;
    }
    printf("%s: %s\n", cn, (map sprintf("m(%s): %s", $1.type(), call(o, $1)), (1, "a", True)).join("; "));
    # the class's own code calls its own variants whatever their access
    if (cn =~ /^Q/) {
        printf("%s via: %s\n", cn, (map sprintf("m(%s): %s", $1.type(), selfcall(o, $1)), (1, "a")).join("; "));
    }
}
