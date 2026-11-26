#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# the checks of users.ql with the classes loaded from the module SameNameClasses;
# same-name-class-type-identity.qtest runs it with the module's source and with a qcc -m .qmod and compares the output
# with probe.expected, the output of the same classes declared in one program (probe.q)

%modern
%no-child-restrictions
%requires SameNameClasses

%include users.ql
