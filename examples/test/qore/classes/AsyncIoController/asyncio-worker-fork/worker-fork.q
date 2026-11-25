#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# run by asyncio_worker_fork.qtest: an asynchronous operation's onComplete(), which a callback worker runs, forks the
# process; each process prints what ran in it.  Argument "complete": the operation completes in its first step;
# "timeout": it waits until its timeout, which the I/O thread cancels it with: its abort() runs on a callback worker,
# which then delivers the result to its onComplete()

%modern

#! an operation whose onComplete() forks
class ForkingOperation inherits AbstractPollOperation<auto> {
    private {
        int parent;
        Counter done;
        bool wait;
        EventNotifier notifier;
    }

    constructor(int parent, Counter done, bool wait, EventNotifier notifier) {
        self.parent = parent;
        self.done = done;
        self.wait = wait;
        self.notifier = notifier;
    }

    string getGoal() {
        return "done";
    }

    string getState() {
        return wait ? "waiting" : "done";
    }

    bool goalReached() {
        return !wait;
    }

    *hash<SocketPollInfo> continuePoll() {
        # the notifier is never signaled: a waiting operation ends with its timeout
        return wait ? <SocketPollInfo>{"events": SOCK_POLLIN, "socket": notifier} : NOTHING;
    }

    #! a Qore abort(): the cancellation of the I/O thread runs it on a callback worker, which then delivers the result
    abort() {
    }

    nothing onComplete(hash<SocketPollResultInfo> result) {
        if (getpid() == parent && !fork()) {
            printf("forked in onComplete()\n");
            flush();
        }
        printf("onComplete() returned in %s\n", getpid() == parent ? "parent" : "child");
        flush();
        if (getpid() == parent) {
            done.dec();
        }
    }

    destructor() {
        printf("operation released in %s\n", getpid() == parent ? "parent" : "child");
        flush();
    }
}

bool wait = (ARGV[0] ?? "complete") == "timeout";
int parent = getpid();
Counter done(1);
EventNotifier n();
AsyncIoController::submit(<SocketPollOperationInfo>{
    "sock": n,
    "spop": new ForkingOperation(parent, done, wait, n),
    "owner": sprintf("worker-fork-%d", parent),
    # a hang guard for the completing operation; the end of the waiting one
    "to": wait ? 100ms : 60s,
    "has_on_complete": True,
});
if (done.waitForZero(60s)) {
    printf("parent: onComplete() did not run\n");
}
printf("parent: script ended\n");
flush();
