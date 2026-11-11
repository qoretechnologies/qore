#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The worker of signal-handler-program-delete.qtest (issue #5766): the closure of a signal handler holds the only
# reference to an object whose destructor uses the data of its Program (a global variable, a static class member and
# a function: signal-handler-program-delete.qi); the Program ends without removing the handler.  The handler must be
# released before the Program's data is deleted, so that the destructor runs with the data, and the process ends with
# status 0.
#
# usage: signal-handler-program-delete-worker.q main|program|teardown|running
#   main: the main Program sets the handler in its %exec-class constructor and ends
#   program: a Program that the main Program creates sets the handler, and the main Program deletes it
#   teardown: the destructor of the %exec-class instance, which runs as the Program's data is destroyed, sets the
#   handler: set_signal_handler() has no effect then, and the closure is released when the destructor returns
#   running: as program, but the Program is deleted while the handler's callback, which has left the Program, is still
#   finishing in the signal thread (held there by a debug hook): the thread that deletes the Program releases the
#   handler (debug builds only)

%modern

%exec-class SignalHandlerProgramDeleteWorker

%include signal-handler-program-delete.qi

#! Sets a handler in its destructor
class TeardownSetter {
    destructor() {
        set_handler();
        printf("the destructor set the handler\n");
    }
}

class SignalHandlerProgramDeleteWorker {
    private {
        #! destroyed with the %exec-class instance, as the Program's data is destroyed
        *TeardownSetter setter;
    }

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

            case "running":
                deleteProgramWhileCallbackFinishes();
                printf("the Program is deleted\n");
                break;

            case "teardown":
                setter = new TeardownSetter();
                printf("main returns\n");
                break;

            default:
                throw "WORKER-ERROR", sprintf("unknown mode %y", ARGV[0]);
        }
    }

    #! Deletes a Program whose signal callback is still finishing in the signal thread when this method returns
    private static deleteProgramWhileCallbackFinishes() {
        Program p(PO_NO_INHERIT_USER_FUNC_VARIANTS | PO_NO_INHERIT_GLOBAL_VARS | PO_NO_INHERIT_USER_CLASSES);
        p.parse(ReadOnlyFile::readTextFile(get_script_dir() + DirSep + "signal-handler-program-delete.qi"),
            "signal-handler-program-delete.qi");
        p.run();
        p.callFunction("set_handler");
        # the signal thread is held once the callback has left the Program, until the deletion of the Program waits
        # for it
        call_function("dbg_signal_hold_after_callback");
        kill(getpid(), SIGTERM);
        if (!call_function("dbg_signal_wait_held_after_callback", 30000)) {
            throw "WORKER-ERROR", "the signal callback did not run";
        }
        printf("deleting the Program while its callback finishes\n");
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
