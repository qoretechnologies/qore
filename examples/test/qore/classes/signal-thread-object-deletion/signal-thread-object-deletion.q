#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# Objects deleted explicitly by a signal handler: the signal handler thread has TID 0, which is the status of an object
# that is not being destroyed (OS_OK); the deletion must mark the object as being destroyed as any other thread does,
# so that a second deletion while the destructor runs (by the destructor itself, or by another thread) is refused with
# DOUBLE-DELETE-EXCEPTION and the destructor runs once (qore#7509).  Each case runs in a background thread as well,
# whose output must be the same.  Needs signal handling (not -b).

%modern

const HoldTimeout = 30s;

#! deletes itself in its destructor
class SelfDeleter {
    public {
        int runs = 0;
        Queue result;
    }

    constructor(Queue result) {
        self.result = result;
    }

    destructor() {
        ++runs;
        string err = "no error";
        if (runs == 1) {
            try {
                delete self;
            } catch (hash<ExceptionInfo> ex) {
                err = ex.err;
            }
        }
        result.push(sprintf("destructor run %d: %s", runs, err));
    }
}

#! waits in its destructor until another thread has tried to delete it
class Waiter {
    public {
        Counter in_dtor(1);
        Counter other_done(1);
        Counter runs();
    }

    destructor() {
        runs.inc();
        in_dtor.dec();
        other_done.waitForZero(HoldTimeout);
    }
}

#! runs \a deleter in the context \a ctx: "thread" (a background thread) or "signal" (a signal handler)
sub run_in(string ctx, code deleter) {
    Counter done(1);
    if (ctx == "thread") {
        background sub () {
            on_exit done.dec();
            deleter();
        }();
    } else {
        set_signal_handler(SIGUSR1, sub (softint sig) {
            on_exit done.dec();
            deleter();
        });
        kill(getpid(), SIGUSR1);
    }
    if (done.waitForZero(HoldTimeout)) {
        printf("%s: the deleting code did not complete\n", ctx);
    }
    if (ctx == "signal") {
        remove_signal_handler(SIGUSR1);
    }
}

#! the destructor deletes its own object
sub self_delete(string ctx) {
    Queue result();
    SelfDeleter o(result);
    run_in(ctx, sub () { delete o; });
    list<string> l = ();
    while (result.size()) {
        l += result.get();
    }
    printf("%s: self delete: %s\n", ctx, l.join("; "));
}

#! another thread deletes the object while its destructor runs
sub other_delete(string ctx) {
    Waiter o();
    Counter in_dtor = o.in_dtor;
    Counter other_done = o.other_done;
    Counter runs = o.runs;
    Queue result();
    # the other thread's own reference: the delete operator takes the object out of the variable that it deletes
    Waiter mine = o;
    background sub () {
        on_exit other_done.dec();
        if (in_dtor.waitForZero(HoldTimeout)) {
            result.push("the destructor did not start");
            return;
        }
        try {
            delete mine;
            result.push("no error");
        } catch (hash<ExceptionInfo> ex) {
            result.push(ex.err);
        }
    }();
    run_in(ctx, sub () { delete o; });
    printf("%s: other thread delete: %s; destructor runs: %d\n", ctx, result.get(), runs.getCount());
}

if (!Option::HAVE_SIGNAL_HANDLING) {
    print("no signal handling\n");
    return;
}

foreach string ctx in ("thread", "signal") {
    self_delete(ctx);
    other_delete(ctx);
}
