#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# the classes of users.ql with parent classes loaded from the modules AbstractModuleParentA and AbstractModuleParentB;
# abstract-module-parent-override.qtest runs it with the modules' sources and with qcc -m .qmods and compares the
# output with probe.expected, the output of the same classes declared in one program (probe.q)

%modern
%disable-warning non-existent-method-call
%requires AbstractModuleParentA
%requires AbstractModuleParentB

%include users.ql
