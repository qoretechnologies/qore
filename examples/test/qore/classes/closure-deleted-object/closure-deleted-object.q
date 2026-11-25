#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# Calls of a closure created in a method while another thread deletes the method's object (qore#7555): a call runs
# whether or not the object has been deleted (issue #1303), and the closure's code raises OBJECT-ALREADY-DELETED only
# where it uses the object.  A debug hook (dbg_hold_closure_call()) holds the calling thread before the call takes a
# reference to the object, and the object is deleted meanwhile, so the program only runs against a debug build.
# With the argument "native" (a program compiled with AOT, whose closure calls enter the closure's object as native
# code does, without a reference and without a test of the object, so that they have no such window), the object is
# deleted before the call instead.

%modern

class T {
    public {
        string name = "t";
    }

    #! returns a closure that does not use the object
    code getClosure() {
        return string sub (string arg) { return "ran " + arg; };
    }

    #! returns a closure that uses the object
    code getSelfClosure() {
        return string sub (string arg) { return name + " " + arg; };
    }
}

const HoldTimeout = 30s;

#! calls the closure \\a c, which another thread deletes the object of before the call takes a reference to it
sub call_while_deleted(string label, T t, code c) {
    if (ARGV[0] == "native") {
        delete t;
        try {
            printf("%s: returned %y\n", label, c("x"));
        } catch (hash<ExceptionInfo> ex) {
            printf("%s: %s\n", label, ex.err);
        }
        return;
    }
    Counter held(1);
    Counter release(1);
    Queue result();
    call_function("dbg_hold_closure_call", "T", held, release);
    background sub () {
        try {
            result.push(sprintf("returned %y", c("x")));
        } catch (hash<ExceptionInfo> ex) {
            result.push(ex.err);
        }
    }();
    if (held.waitForZero(HoldTimeout)) {
        printf("%s: the call was not held\n", label);
        release.dec();
        printf("%s: %s\n", label, result.get());
        return;
    }
    delete t;
    release.dec();
    printf("%s: %s\n", label, result.get());
}

if (!Option::HAVE_LIBRARY_DEBUGGING) {
    printf("the debug hooks are only in debug builds\n");
    exit(0);
}

{
    T t();
    call_while_deleted("deleted, closure without self", t, t.getClosure());
}
{
    T t();
    call_while_deleted("deleted, closure with self", t, t.getSelfClosure());
}
{
    T t();
    code c = t.getClosure();
    printf("not deleted: %s\n", c("y"));
    delete t;
    printf("deleted before: %s\n", c("z"));
}
