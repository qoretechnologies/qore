#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# the classes of users.ql with parent classes loaded from the module AbstractTypedCallA;
# abstract-typed-call-created-method.qtest runs it with the module's source and with a qcc -m .qmod and compares the
# output with probe.expected, the output of the same classes declared in one program (probe.q)

%modern
%requires AbstractTypedCallA

%include users.ql
