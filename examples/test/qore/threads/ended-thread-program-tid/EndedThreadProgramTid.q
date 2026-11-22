# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The child of ended-thread-program-tid.qtest (#7103): a thread that entered a Program ends while the Program is torn
# down; the Program's records of the thread then name the thread by its TID after the thread's data has been freed.
#
# usage: EndedThreadProgramTid.q teardown|debugger
#
# Both cases end the thread from the destructor of an object in a global variable of the Program, which the teardown
# runs in the thread that deletes the Program (while it deletes the Program's global variables) after it has started
# clearing the data of the Program's threads (tclear): the thread's end does not remove its entry from the Program
# (qore_program_private::endThread() fails), so the Program keeps the thread's record (ThreadProgramData) while the
# thread's data (ThreadData) is freed when the thread ends.  The destructor joins the thread (thread_join() returns
# once the thread's data has been freed), so the thread has ended, and its data has been freed, before the teardown
# continues.  The thread is a thread of this Program that only called a function of the torn down Program and left
# it, so the teardown does not wait for it (as it does for the threads that run in the Program).

%modern
%allow-debugger
%no-child-restrictions

#! QPRS_THREAD_DATA_CLEAR_START: the start of the clearing of the data of a Program's threads
const SiteClearStart = 7;
#! QPRS_THREAD_DATA_DELETED: after the data of a thread in a Program has been deleted while the Program is cleared
const SiteThreadDataDeleted = 9;

const ProgramCode = "
our object holder;
int sub f() { return 1; }
";

#! a debugger that does nothing
class Debugger inherits DebugProgram {
    onAttach(ProgramControl pgm, reference rs, reference rts) {
    }
    onDetach(ProgramControl pgm, reference rs, reference rts) {
    }
    onStep(ProgramControl pgm, int bid, *int sid, *int bkid, reference flow, reference rs, reference rts) {
    }
    onFunctionEnter(ProgramControl pgm, int sid, reference rs, reference rts) {
    }
    onFunctionExit(ProgramControl pgm, int sid, reference result, reference rs, reference rts) {
    }
    onException(ProgramControl pgm, int sid, hash<auto> ex, reference dismiss, reference rs, reference rts) {
    }
    onExit(ProgramControl pgm, int sid, reference result, reference rs, reference rts) {
    }
}

#! a thread that enters the Program once (a call of f()), leaves it and ends when asked
class Caller {
    public {
        int tid;
    }

    private {
        Queue end_queue();
    }

    constructor(Program pgm) {
        Queue started();
        tid = background sub () {
            started.push(pgm.callFunction("f"));
            end_queue.get();
        }();
        if (started.get() != 1) {
            throw "TEST-ERROR", "the thread did not call into the Program";
        }
    }

    #! asks the thread to end and waits until it has ended and its data has been freed
    end() {
        end_queue.push(True);
        if (!thread_join(tid)) {
            throw "TEST-ERROR", sprintf("TID %d was not running", tid);
        }
    }
}

#! ends the thread from the teardown of the Program, after it has started clearing the data of the Program's threads
class Ender {
    private {
        Caller caller;
        *code in_window;
    }

    constructor(Caller caller, *code in_window) {
        self.caller = caller;
        self.in_window = in_window;
    }

    destructor() {
        if (in_window) {
            in_window(\caller.end());
        } else {
            caller.end();
        }
        printf("thread ended in the teardown\n");
    }
}

#! the TID that the teardown reads for the ended thread when it deletes the thread's data is the thread's TID
/** The teardown records the deletion of the data of each thread with the TID that it reads from the Program's record
    of the thread (qore_program_private::clearProgramThreadData(), QORE_DBG_RECORD_CHANGE_TID()).  A hold at that
    change, armed for the thread's TID, parks the teardown only if the TID that it read is the thread's; before the
    fix, the TID was read from the freed data of the thread (ThreadProgramData::gettid()), which MALLOC_PERTURB_ fills
    with another value, and the teardown went on without parking.
*/
sub teardown_case() {
    *Program p = new Program(PO_NEW_STYLE);
    p.parse(ProgramCode, "ended-thread-program-tid");
    Caller caller(p);
    int tid = caller.tid;
    p.setGlobalVarValue("holder", new Ender(caller));
    remove caller;

    Queue events();
    call_function("dbg_hold_program_record_change", SiteThreadDataDeleted, p, tid);
    # the teardown runs in a thread of its own, as it parks there if it reads the thread's TID
    int deleter = background sub (Program pgm) {
        delete pgm;
        events.push("deleted");
    }(p);
    remove p;
    # reports when the teardown has parked; in the case that it never parks, it is woken by the hold of another
    # Program below
    int waiter = background sub () {
        events.push(call_function("dbg_wait_program_record_held") ? "held" : "not held");
    }();

    # the teardown either parks at the deletion of the ended thread's data (and cannot finish the deletion until it
    # is released) or finishes the deletion without parking
    string first = events.get();
    call_function("dbg_hold_program_record_change_clear");
    if (first == "held") {
        # the teardown continues now
        string second = events.get();
        if (second != "deleted") {
            throw "TEST-ERROR", sprintf("unexpected event %y after the hold", second);
        }
    } else {
        # the waiter is still waiting: wake it with a hold that the teardown of another Program reaches
        Program other(PO_NEW_STYLE);
        call_function("dbg_hold_program_record_change", SiteClearStart, other);
        int other_deleter = background sub (Program pgm) {
            delete pgm;
        }(other);
        remove other;
        string second = events.get();
        call_function("dbg_hold_program_record_change_clear");
        thread_join(other_deleter);
        if (second != "held") {
            throw "TEST-ERROR", sprintf("unexpected event %y after the deletion", second);
        }
    }
    thread_join(deleter);
    thread_join(waiter);
    printf("teardown recorded the thread's TID: %y\n", first == "held");
}

#! a debugger offers the ended thread again while the teardown still holds the thread's data
/** DebugProgram::attachThread() finds the thread in the Program by its TID (qore_program_private::attachThread()); it
    is called in the teardown after the thread has ended, while the Program still holds the thread's data, so it finds
    the thread.  Before the fix, it read the TID of each thread from the thread's data, which MALLOC_PERTURB_ fills
    with another value once the thread has ended and its data has been freed, and it did not find the thread.  Also a
    release build reads it so.
*/
sub debugger_case() {
    Program p(PO_NEW_STYLE | PO_ALLOW_DEBUGGER);
    p.parse(ProgramCode, "ended-thread-program-tid");
    Caller caller(p);
    int tid = caller.tid;
    ProgramControl pc = p.getProgram();
    Debugger d();
    *bool attached;
    *bool attached_before;
    *int break_thread;
    p.setGlobalVarValue("holder", new Ender(caller, sub (code end) {
        # the Program is added to the debugger in its teardown (the teardown removes it from its debugger first), while
        # the thread is still running: the debugger is offered the Program's threads then
        d.addProgram(pc);
        attached_before = d.attachThread(pc, tid);
        end();
        attached = d.attachThread(pc, tid);
        # a pause of the Program and of the thread read whether the ended thread can run debugger callbacks, also
        # from the Program's record of the thread (qore#7103): neither stops it, as it is no longer in the Program
        d.breakProgram(pc);
        break_thread = d.breakProgramThread(pc, tid);
    }));
    remove caller;
    delete p;
    d.removeProgram(pc);
    printf("attachThread() before the thread ended: %y\n", attached_before);
    printf("attachThread() after the thread ended: %y\n", attached);
    printf("breakProgramThread() after the thread ended: %y\n", break_thread);
}

switch (ARGV[0]) {
    case "teardown": teardown_case(); break;
    case "debugger": debugger_case(); break;
    default: throw "TEST-ERROR", sprintf("unknown case %y", ARGV[0]);
}
