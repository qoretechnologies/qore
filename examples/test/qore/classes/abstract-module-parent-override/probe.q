#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# the classes of the modules AbstractModuleParentA and AbstractModuleParentB and of users.ql declared in one program;
# abstract-module-parent-override.qtest compares the output of every execution mode, of qcc executables and of
# module-user.q, whose parent classes are loaded from the modules, with probe.expected

%modern
%disable-warning non-existent-method-call

namespace AMA {
class I1 { abstract string m(int i); }
class I2 { abstract string m(string s); }
class I3 { abstract string m(bool b); }
class Sint { string m(int i) { return "Sint::m(int)"; } }
class Q inherits I1, Sint { abstract string m(string s); }
}

namespace AMB {
class N8 inherits AMA::Q { string m(string s) { return "N8::m(string)"; } }
class N9 inherits private N8, AMA::I2, AMA::I1, AMA::Sint { string m(string s) { return "N9::m(string)"; } }
}

%include users.ql
