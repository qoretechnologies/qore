#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# run by asyncio_cancel_fork.qtest: deletes a Program whose asynchronous operation forks the process in its abort()
# (argument "abort") or in its destructor (argument "destructor") while the deletion cancels it; each process prints
# what ran in it, and the child ends after the deletion

%modern

string fork_at = ARGV[0] ?? "abort";
int parent = getpid();

Program p(PO_NEW_STYLE);
p.setScriptPath(get_script_path());
p.parse("
# an object released after the operation; its destructor tells which process released it
class Marker {
    private {
        int parent;
    }

    constructor(int parent) {
        self.parent = parent;
    }

    destructor() {
        printf(\"marker released in %s\\n\", getpid() == parent ? \"parent\" : \"child\");
        flush();
    }
}

# an operation that waits for an event of its notifier, which never comes: the deletion of the Program cancels it
class ForkingOperation inherits AbstractPollOperation<auto> {
    # the members, not global variables, which the deletion of the Program can clear before it cancels the operation
    private {
        EventNotifier notifier;
        int parent;
        string fork_at;
    }

    constructor(EventNotifier notifier, int parent, string fork_at) {
        self.notifier = notifier;
        self.parent = parent;
        self.fork_at = fork_at;
    }

    string getGoal() {
        return \"waiting\";
    }

    string getState() {
        return \"waiting\";
    }

    bool goalReached() {
        return False;
    }

    *hash<SocketPollInfo> continuePoll() {
        return <SocketPollInfo>{\"events\": SOCK_POLLIN, \"socket\": notifier};
    }

    abort() {
        if (fork_at == \"abort\" && getpid() == parent) {
            if (!fork()) {
                printf(\"forked in abort()\\n\");
                flush();
            }
        }
    }

    destructor() {
        if (fork_at == \"destructor\" && getpid() == parent) {
            if (!fork()) {
                printf(\"forked in the destructor\\n\");
                flush();
            }
        }
        printf(\"operation released in %s\\n\", getpid() == parent ? \"parent\" : \"child\");
        flush();
    }
}

EventNotifier n;

sub start(int parent, string fork_at) {
    n = new EventNotifier();
    Queue<hash<SocketPollStateInfo>> states();
    AsyncIoController::submit(<SocketPollOperationInfo>{
        \"sock\": n,
        \"spop\": new ForkingOperation(n, parent, fork_at),
        \"owner\": sprintf(\"cancel-fork-%d\", parent),
        \"to\": -1s,
        \"other\": {\"marker\": new Marker(parent)},
        \"stateQueue\": states,
    });
    # the I/O thread pushes the state of the first step when it processes the step's result, and answers getInfo()
    # after it: the operation then waits for its notifier, with no step in flight, so that the deletion of the
    # Program cancels it in the deleting thread, which runs its abort()
    states.get(60s);
    AsyncIoController::getInfo();
}
", "cancel-fork");
p.callFunction("start", parent, fork_at);

# cancels the Program's operation: its abort() and its release run in this thread
delete p;

printf("%s: deletion returned\n", getpid() == parent ? "parent" : "child");
flush();
