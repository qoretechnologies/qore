#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# the classes of the module AbstractPublicOverloadA and of users.ql declared in one program;
# abstract-public-overload-private-impl.qtest compares the output of every execution mode, of qcc executables and of
# module-user.q, whose parent classes are loaded from the module, with probe.expected

%modern
%disable-warning non-existent-method-call

namespace APO {
class C5 { string m(int i) { return "C5::m(int)"; } }
class P3 { string m(bool b) { return "P3::m(bool)"; } }
class P3p { private string m(bool b) { return "P3p::m(bool)"; } }
class G0 {
    abstract string m(bool b);
    abstract string m(int i);
    string cm(auto a) { return m(a); }
}
class G3 inherits G0 { string m(bool b) { return "G3::m(bool)"; } }
class F0 {
    abstract private string m(bool b);
    abstract private string m(int i);
    string cm(auto a) { return m(a); }
}
class F3 inherits F0 { string m(bool b) { return "F3::m(bool)"; } }
class K inherits private:internal C5 { string k() { return "K"; } }
}

%include users.ql
