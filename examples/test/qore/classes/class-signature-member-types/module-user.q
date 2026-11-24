#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# the checks of checks.ql with the classes of classes.ql loaded from the module MemberTypeClasses;
# class-signature-member-types.qtest runs it with the module's source and with a qcc -m .qmod and compares the output
# with probe.expected; the first argument is the directory of classes.ql

%modern
%no-child-restrictions
%requires MemberTypeClasses

%include checks.ql
