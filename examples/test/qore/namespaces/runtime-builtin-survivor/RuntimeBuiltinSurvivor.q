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
    case "cycle": {
        # two Programs whose classes, added at run time, name each other: they keep each other until library cleanup,
        # which frees them (qore#7680)
        Program a(PO_NEW_STYLE);
        a.parse("sub naming(Program p, int k) { dbg_add_runtime_naming(p, k); }", "cycle");
        Program b(PO_NEW_STYLE);
        a.callFunction("naming", b, 5);
        delete a;
        delete b;
        if (dbg_cross_program_cycles() != 2) {
            printf("no cycle of two\n");
            exit(5);
        }
        break;
    }
    case "shared": {
        # a variant of the static system namespace keeps a class added at run time until library cleanup (qore#7680)
        Program a(PO_NEW_STYLE);
        a.parse("sub add() { dbg_add_runtime_namespace(\"DbgRtNs\", 1); }
sub naming(Program p, int k) { dbg_add_runtime_naming(p, k); }", "shared");
        a.callFunction("add");
        Program q(PO_NEW_STYLE);
        q.parse("sub naming(Program p, int k) { dbg_add_runtime_naming(p, k); }", "q");
        q.callFunction("naming", a, 3);
        # and a class of the static system namespace names it without a reference
        a.callFunction("naming", a, 4);
        delete a;
        delete q;
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
