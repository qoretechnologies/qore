#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# the classes of the module SameNameClasses and the checks of users.ql in one program;
# same-name-class-type-identity.qtest compares the output of every execution mode, of qcc executables and of
# module-user.q, which loads the classes from the module, with probe.expected

%modern
%no-child-restrictions

namespace SNA {
class Foo {}
}

namespace SNB {
class Foo {}
}

namespace SNM {
class C {
    string m(SNA::Foo f) { return "m(SNA::Foo)"; }
    string m(SNB::Foo f) { return "m(SNB::Foo)"; }
}
}

%include users.ql
