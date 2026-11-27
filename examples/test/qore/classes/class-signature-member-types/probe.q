#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# the classes of classes.ql and the checks of checks.ql in one program; class-signature-member-types.qtest compares
# the output of every execution mode, of qcc executables and of module-user.q, which loads the classes from the
# module MemberTypeClasses, with probe.expected; the first argument is the directory of classes.ql

%modern
%no-child-restrictions

%include classes.ql

%include checks.ql
