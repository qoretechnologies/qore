#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# the calls of users.ql with the classes loaded from the module OwnInternalTypedCallA; own-private-internal-method-
# typed-call.qtest runs it with the module's source and with a qcc -m .qmod and compares the output with
# probe.expected, the output of the same classes declared in one program (probe.q)

%modern
%requires OwnInternalTypedCallA

%include users.ql
