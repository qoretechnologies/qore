#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The worker of signal-handler-program-delete.qtest (issue #5766): the closure of a signal handler holds the only
# reference to an object whose destructor uses the data of its Program (a global variable, a static class member and
# a function: signal-handler-program-delete.qi); the Program ends without removing the handler.  The handler must be
# released before the Program's data is deleted, so that the destructor runs with the data, and the process ends with
# status 0.
#
# usage: signal-handler-program-delete-worker.q main|program
#   main: the main Program sets the handler in its %exec-class constructor and ends
#   program: a Program that the main Program creates sets the handler, and the main Program deletes it

%modern

%exec-class SignalHandlerProgramDeleteWorker

%include signal-handler-program-delete.qi

class SignalHandlerProgramDeleteWorker {
    constructor() {
        if (!Option::HAVE_SIGNAL_HANDLING) {
            printf("no signal handling\n");
            return;
        }
        switch (ARGV[0]) {
            case "main":
                set_handler();
                printf("main returns\n");
                break;

            case "program":
                deleteProgram();
                printf("the Program is deleted\n");
                break;

            default:
                throw "WORKER-ERROR", sprintf("unknown mode %y", ARGV[0]);
        }
    }

    #! Sets the handler in a Program of its own, which is deleted when this method returns
    private static deleteProgram() {
        Program p(PO_NO_INHERIT_USER_FUNC_VARIANTS | PO_NO_INHERIT_GLOBAL_VARS | PO_NO_INHERIT_USER_CLASSES);
        p.parse(ReadOnlyFile::readTextFile(get_script_dir() + DirSep + "signal-handler-program-delete.qi"),
            "signal-handler-program-delete.qi");
        p.run();
        p.callFunction("set_handler");
        printf("deleting the Program\n");
    }
}
