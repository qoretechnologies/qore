#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# the classes of the module AbstractTypedCallA and of users.ql declared in one program;
# abstract-typed-call-created-method.qtest compares the output of every execution mode, of qcc executables and of
# module-user.q, whose parent classes are loaded from the module, with probe.expected

%modern

namespace ATC {
class C5 { string m(int i) { return "C5::m(int)"; } }
class G0 {
    abstract string m(bool b);
    abstract string m(int i);
}
class G3 inherits G0 { string m(bool b) { return "G3::m(bool)"; } }
}

%include users.ql
