#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# run by entry-cpp-exception.qtest: enters Program p from Qore code while a step of the entry fails with a C++
# exception (std::bad_alloc, made by dbg_fail_next_program_entry(); debug builds only), and prints what the caller
# got; usage: entry-cpp-exception-child.q <scenario> <site>

%modern

%exec-class EntryCppExceptionChild

class EntryCppExceptionChild {
    private {
        Program p(PO_NEW_STYLE);
        code closure;
        object obj;
    }

    constructor() {
        string scenario = ARGV[0];
        int site = ARGV[1].toInt();

        p.parse("%modern
int sub f() { return 1; }
list<code> sub handlers() {
    return (
        string sub () { return \"token\"; },
        sub (string token) {},
    );
}
class C { int m() { return 2; } }
code sub closure() { return int sub () { return 3; }; }
C sub obj() { return new C(); }
", "p");
        list<code> h = p.callFunction("handlers");
        closure = p.callFunction("closure");
        obj = p.callFunction("obj");
        p.setThreadEntryHandler(h[0], h[1]);

        if (scenario == "submitTask") {
            submitTask(site);
            return;
        }
        code call;
        switch (scenario) {
            case "callFunction": call = auto sub () { return p.callFunction("f"); }; break;
            case "closure": call = auto sub () { return closure(); }; break;
            case "method": call = auto sub () { return obj.m(); }; break;
        }
        call_function("dbg_fail_next_program_entry", site);
        try {
            printf("no exception: %y\n", call());
        } catch (hash<ExceptionInfo> ex) {
            printf("caught %s\n", ex.err);
        }
        # the Program can be entered again
        printf("again: %y\n", call());
    }

    #! an async I/O task calls a closure of the Program in a worker thread, which enters the Program
    /** The task gets the exception; a second task, which runs once the first one has ended, enters the Program again
    */
    private submitTask(int site) {
        foreach bool armed in ((True, False)) {
            Counter done(1);
            *string result;
            if (armed) {
                call_function("dbg_fail_next_program_entry", site);
            }
            AsyncIoController::submitTask(sub () {
                on_exit done.dec();
                try {
                    result = sprintf("again: %y", closure());
                } catch (hash<ExceptionInfo> ex) {
                    result = "caught " + ex.err;
                }
            });
            done.waitForZero();
            printf("%s\n", result);
        }
    }
}
