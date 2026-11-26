#!/usr/bin/env qore
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the child process of runtime-builtin-survivor.qtest: a value that names a class that the module rtsurv added to a
# Program at run time is kept to library cleanup (dbg_keep_until_cleanup()), after the module is unloaded (qore#7540)

%modern
%requires rtsurv

string which = ARGV[0];
Program p(PO_NEW_STYLE);
p.parse("sub add() { rtsurv_add(\"RtSurvNs\"); }
object sub child() {
    Program c(PO_NEW_STYLE);
    c.parse('object sub mk() { return new RtSurvNs::RtSurvClass(); }
code sub ref() { return \\RtSurvNs::rtsurv_f(); }
string sub sv() { return RtSurvNs::RtSurvClass::sv; }', 'child');
    return c;
}", "surv");
p.callFunction("add");
object c = p.callFunction("child");
if (c.callFunction("sv") != "module") {
    printf("wrong static value\n");
    exit(2);
}
switch (which) {
    case "object": {
        object o = c.callFunction("mk");
        if (o.get() != 1) {
            printf("wrong live count\n");
            exit(3);
        }
        dbg_keep_until_cleanup(o);
        break;
    }
    case "callref": {
        code r = c.callFunction("ref");
        if (r(NOTHING) != 1) {
            printf("wrong call result\n");
            exit(4);
        }
        dbg_keep_until_cleanup(r);
        break;
    }
    case "program":
        dbg_keep_until_cleanup(c);
        break;
    case "static": {
        # a Program without the module's classes gets the class added at run time, whose method names the class of
        # the module's namespace; the Program is kept beyond the deletion of the module (qore#7540)
        Program s(PO_NEW_STYLE | PO_NO_INHERIT_SYSTEM_CLASSES);
        # the module's function runs in the Program that calls it, which has the module's functions but not its classes
        s.parse("sub add() { rtsurv_add(\"RtSurvNsS\"); }", "static");
        s.callFunction("add");
        dbg_keep_until_cleanup(s);
        break;
    }
    case "one": {
        # as "static", but the class that names the module's class is added one at a time (addSystemClass()) to a
        # namespace that the Program already has (qore#7649)
        Program s(PO_NEW_STYLE | PO_NO_INHERIT_SYSTEM_CLASSES);
        s.parse("sub add() { rtsurv_add(\"RtSurvNsO\"); rtsurv_add_one(\"RtSurvNsO\"); }", "one");
        s.callFunction("add");
        dbg_keep_until_cleanup(s);
        break;
    }
    case "both": {
        # an object, whose destructor is the module's, and the Program that shares the class and its static variables
        object o = c.callFunction("mk");
        dbg_keep_until_cleanup((o, c));
        break;
    }
}
remove c;
delete p;
printf("ok\n");
