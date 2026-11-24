#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# the classes of the module AbstractRedeclaredOrder and of users.ql declared in one program;
# abstract-redeclared-parent-order.qtest compares the output of every execution mode, of qcc executables and of
# module-user.q, whose parent classes are loaded from the module, with probe.expected

%modern
%disable-warning non-existent-method-call

namespace ARO {
class C0 { abstract private string m(); }
class C3 inherits C0 { string m() { return "C3::m"; } }
class C4 inherits C3 { abstract string m(); }
class C4p inherits C3 { abstract private string m(); }
class C4h inherits C3 { abstract private:hierarchy string m(); }
class C4i inherits C3 { abstract private:internal string m(); }
class C5 inherits C4 {}
class C6 inherits C5 { abstract string m(); }
class X0 { abstract string m(); }
class X3 { string m() { return "X3::m"; } }
class X4 inherits private:internal X3, X0 { abstract string m(); }
class B0 {
    abstract string m(int i);
    abstract string m(string s);
}
class B3 inherits B0 {
    string m(int i) { return "B3::m(int)"; }
    string m(string s) { return "B3::m(string)"; }
}
class B4 inherits B3 { abstract string m(int i); }
class P0 { abstract string m(bool b) const; }
class P1 inherits private:internal P0 { abstract string m(bool b) const; }
class P2 inherits private:internal P0, P1 { string m(bool b) const { return "P2::m(bool) const"; } }
class P5 inherits P1, P2 { abstract string m(bool b) const; }
class N7 inherits C4, C0 { string via() { return m(); } }
class N8 inherits C0, C4 { string via() { return m(); } }
class N9 inherits P5, P1 {}
}

%include users.ql
