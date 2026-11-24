#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# the classes of the module PrivateInternalSubclassA and of users.ql declared in one program;
# private-internal-subclass-calls.qtest compares the output of every execution mode, of qcc executables and of
# module-user.q, whose parent classes are loaded from the module, with probe.expected

%modern
%disable-warning non-existent-method-call

namespace PIS {
class C5 {
    string m(int i) { return "C5::m(int)"; }
    static string s(int i) { return "C5::s(int)"; }
}
class P3 {
    string m(bool b) { return "P3::m(bool)"; }
    string pdyn(auto a) { object o = self; return o.m(a); }
}
class Y { string m(int i) { return "Y::m(int)"; } }
class A {
    string m(string s) { return "A::m(string)"; }
    string adyn(auto a) { object o = self; return o.m(a); }
}
class K inherits private:internal C5 {
    string kdyn(auto a) { object o = self; return o.m(a); }
    string kcom(auto a) { return call_object_method(self, "m", a); }
    string kcoma(auto a) { return call_object_method_args(self, "m", (a,)); }
    string kref(auto a) { code c = self.getCallReference("m"); return c(a); }
}
}

%include users.ql
