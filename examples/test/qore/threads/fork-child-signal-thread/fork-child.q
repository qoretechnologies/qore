#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# run by fork-child-signal-thread.qtest, with signal handling enabled or with -b: a thread other than the main thread
# forks the process and returns in the child; the child must end when its last thread that runs Qore code has ended
# (qore#7613).  Argument "thread": the child's forking thread starts another thread first, which ends after it (it
# joins the forking thread, so that the forking thread is not the child's last thread); "grandchild": the child's
# forking thread forks again and returns in both processes; "worker": the forking thread is an asynchronous I/O
# callback worker (a thread with external lifecycle management) running an operation's onComplete().  The child writes
# its output without flushing it: the output must be written when the child ends.  The parent waits for the end of the
# child with a pipe whose writing end the child keeps until it ends, and ends the child if it does not end (a hang
# guard, not a timing assertion), with the processes that it forked (the child leads a process group of its own)

%modern

const HangGuard = 60s;

hash<PipeInfo> p = File::getPipe();
string mode = ARGV[0] ?? "";
int parent = getpid();
Queue started();

#! an operation that completes in its first step; its onComplete(), run by a callback worker, forks
class ForkingOperation inherits AbstractPollOperation<auto> {
    private {
        #! gets the PID of the child
        Queue started;
    }

    constructor(Queue started) {
        self.started = started;
    }

    string getGoal() {
        return "done";
    }

    string getState() {
        return "done";
    }

    bool goalReached() {
        return True;
    }

    *hash<SocketPollInfo> continuePoll() {
    }

    nothing onComplete(hash<SocketPollResultInfo> result) {
        int pid = fork();
        if (!pid) {
            # the child: the worker is its only thread that runs Qore code
            setsid();
            printf("child: worker returns\n");
            return;
        }
        started.push(pid);
    }
}

# the socket of the operation of the "worker" mode, which lives as long as the script
EventNotifier n();
if (mode == "worker") {
    AsyncIoController::submit(<SocketPollOperationInfo>{
        "sock": n,
        "spop": new ForkingOperation(started),
        "owner": sprintf("fork-child-%d", parent),
        "to": HangGuard,
        "has_on_complete": True,
    });
} else {
    background sub () {
        int pid = fork();
        if (!pid) {
            # the child: this is its only thread that runs Qore code; a grandchild is in its process group
            setsid();
            if (mode == "grandchild") {
                # forked by the only thread of a child that has no main thread
                if (!fork()) {
                    printf("grandchild: forking thread returns\n");
                    return;
                }
            }
            if (mode == "thread") {
                int fork_tid = gettid();
                background sub () {
                    # ends after the forking thread has ended
                    if (!thread_join(fork_tid, HangGuard)) {
                        printf("child: the forking thread did not end\n");
                    }
                    printf("child: thread ended\n");
                }();
            }
            printf("child: forking thread returns\n");
            return;
        }
        started.push(pid);
    }();
}
int pid = started.get(HangGuard);
# the parent's writing end: the reading end gets the end of the input when the child's end is closed, as it ends
p.write.close();
if (p.read.isDataAvailable(HangGuard) && !exists p.read.read(1)) {
    printf("parent: the child ended\n");
} else {
    printf("parent: the child did not end\n");
    if (pid > 0) {
        # the child's process group, with a grandchild, and the child if it did not get to lead one
        kill(-pid, SIGKILL);
        kill(pid, SIGKILL);
    }
}
flush();
