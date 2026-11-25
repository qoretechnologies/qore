#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# calls the methods of the classes of the AbstractPrivDecl module from outside their hierarchy (qore#7288); the test
# runs it with the module's source and with a qcc -m .qmod and compares the output with module-user.expected

%modern
%requires AbstractPrivDecl

foreach string cn in ("X1", "Y1", "A5", "XC3", "X2", "A1", "A2", "XC1", "XC2") {
    object o = create_object("APD::" + cn);
    auto a = o;
    try {
        printf("%s: %y\n", cn, a.m());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", cn, ex.err);
    }
    try {
        printf("%s call_object_method: %y\n", cn, call_object_method(o, "m"));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s call_object_method: %s\n", cn, ex.err);
    }
}
