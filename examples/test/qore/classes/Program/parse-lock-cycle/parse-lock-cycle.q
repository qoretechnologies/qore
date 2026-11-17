#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# waits for the parse locks of Programs whose commits run code that parses code in other Programs (issue #5693); run
# by parse-lock-cycle.qtest in every execution mode.  Each case prints one line; every wait is bounded, and a case
# that does not end in time (the threads of a deadlock) ends the process with exit(), after printing the cases that
# ended, so that the test sees the failure instead of waiting for the process

%modern
%no-child-restrictions
%enable-all-warnings

#! the Programs of the current case, by name
our hash<string, Program> Peers;
#! the rendezvous of the commits of the current case, before they parse code in other Programs
our Counter Sync;
#! the result of each commit's initializer
our Queue Results;
#! released when the commit that holds the lock of the cancellation case may end
our Counter Release;
#! released by the thread of the cancellation case just before it waits for the parse lock
our Counter Waiting;

#! how long a case may take before it counts as a deadlock
const CaseTimeout = 30s;

#! returns the Program with the given name; imported into the committed Programs
Program sub peer(string name) {
    return Peers{name};
}

#! waits until every commit of the case runs its initializer, so that each holds its Program's parse lock; imported
#! into the committed Programs
sub rendezvous() {
    Sync.dec();
    if (Sync.waitForZero(CaseTimeout)) {
        throw "TEST-ERROR", "the commits did not meet";
    }
}

#! records the result of an initializer; imported into the committed Programs
sub report(string name, string result) {
    Results.push(name + ": " + result);
}

#! waits until the cancellation case releases the commit that holds the parse lock; imported into the Program
sub hold() {
    Sync.dec();
    Release.waitForZero(CaseTimeout);
}

#! the code of each committed Program: its static initializer parses code in the Program that peer_of() names, after
#! waiting \a $1 ms
const Source = "%%new-style
class X {
    public {
        static string v = X::go();
    }
    static string go() {
        string name = get_script_name();
        rendezvous();
        usleep(%d * 1000);
        string rv;
        *string target = %s;
        if (target) {
            try {
                peer(target).parsePending('int sub from_' + name + '() { return 1; }', 'from-' + name);
                rv = 'parsed into ' + target;
            } catch (hash<ExceptionInfo> ex) {
                rv = ex.err;
                if (getenv('PLC_DEBUG')) { stderr.printf(\"%%s: %%s: %%s\\n\", name, ex.err, ex.desc); }
            }
        } else {
            rv = 'nothing parsed';
        }
        report(name, rv);
        return rv;
    }
}
";

#! creates the Programs of a case
sub make_peers(list<string> names) {
    Peers = {};
    foreach string name in (names) {
        Program p(PO_NEW_STYLE | PO_ALLOW_REPARSE);
        p.importFunction("peer");
        p.importFunction("rendezvous");
        p.importFunction("report");
        p.importFunction("hold");
        p.setScriptPath("/parse-lock-cycle/" + name);
        Peers{name} = p;
    }
}

#! ends the process if the threads of a case did not end in time
sub check_done(Counter done, string case_name) {
    if (done.waitForZero(CaseTimeout)) {
        printf("%s: the commits did not end (deadlock)\n", case_name);
        flush();
        exit(1);
    }
}

#! parses the code in a Program of a case; run in a thread of its own
sub commit_one(string name, string src, Counter done) {
    on_exit done.dec();
    try {
        Peers{name}.parse(src, name);
    } catch (hash<ExceptionInfo> ex) {
        Results.push(sprintf("%s: commit: %s: %s", name, ex.err, ex.desc));
    }
}

#! commits the Programs of a case at once, each in a thread of its own, and prints the sorted results
/** @param waits the delay of each Program's initializer after the rendezvous, in ms
    @param targets the Program each Program's initializer parses code in, or none
    @param anonymous if True, the results are printed without the names of the Programs, for a cycle, whose
    refused wait depends on timing
*/
sub commit_case(string case_name, hash<string, int> waits, hash<string, *string> targets, *bool anonymous) {
    make_peers(keys waits);
    Sync = new Counter(waits.size());
    Results = new Queue();
    Counter done(waits.size());
    foreach string name in (keys waits) {
        # the arguments of a background call are evaluated in this thread
        background commit_one(name, sprintf(Source, waits{name}, targets{name} ? sprintf("'%s'", targets{name})
            : "NOTHING"), done);
    }
    check_done(done, case_name);
    list<string> res = map Results.get(), xrange(Results.size());
    if (anonymous) {
        res = map regex_subst($1, "^[a-z0-9]+: (.*?)( into [a-z0-9]+)?$", "$1"), res;
    }
    res = sort(res);
    printf("%s: %s\n", case_name, res.join("; "));
}

#! a Program's initializer parses code in the Program itself: its own parse lock
sub self_case() {
    commit_case("self", {"s": 0}, {"s": "s"});
}

#! the initializer of p parses code in q, whose commit waits for r's: the owner of q's parse lock waits for another
#! parse lock itself (when the delays give the waits this order), which is no cycle, so all waits end
sub chain_case() {
    commit_case("chain", {"p": 200, "q": 0, "r": 500}, {"p": "q", "q": "r", "r": NOTHING});
}

#! the initializers of p and q each parse code in the other's Program: the wait that closes the cycle is refused, and
#! the other one ends when the refused commit ends
sub cross_case() {
    commit_case("cross", {"p": 0, "q": 0}, {"p": "q", "q": "p"}, True);
}

#! a cycle through three Programs: one wait is refused, and the others end
sub cycle3_case() {
    commit_case("cycle3", {"p": 0, "q": 0, "r": 0}, {"p": "q", "q": "r", "r": "p"}, True);
}

#! parses code in the Program of the case that ends a wait; imported into the sandboxed Program of the interrupt case
string sub wait_parse() {
    try {
        Waiting.dec();
        Peers.c.parsePending("int sub w() { return 1; }", "w");
        return "parsed";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! a thread that waits for the parse lock that a commit holds is cancelled, or interrupted by the sandbox manager of
#! the Program whose code it runs
sub end_wait_case(string case_name, bool interrupt) {
    make_peers(("c",));
    Sync = new Counter(1);
    Release = new Counter(1);
    Results = new Queue();
    Counter done(1);
    # the commit holds the lock until it is released
    background commit_one("c", "%new-style
class X {
    public {
        static bool v = X::go();
    }
    static bool go() {
        hold();
        return True;
    }
}", done);
    if (Sync.waitForZero(CaseTimeout)) {
        printf("%s: the commit did not start\n", case_name);
        flush();
        exit(1);
    }
    *SandboxManager sm;
    *Program sandboxed;
    if (interrupt) {
        sm = new SandboxManager();
        sandboxed = new Program(PO_NEW_STYLE);
        sandboxed.setSandboxManager(sm);
        sandboxed.importFunction("wait_parse");
        sandboxed.parse("string sub go() { return wait_parse(); }", "sandboxed");
    }
    Waiting = new Counter(1);
    Counter waited(1);
    string result = "no result";
    int tid = background sub () {
        on_exit waited.dec();
        try {
            result = sandboxed ? sandboxed.callFunction("go") : wait_parse();
        } catch (hash<ExceptionInfo> ex) {
            result = "after the wait: " + ex.err;
        }
    }();
    # the thread is about to wait for the lock, and then has the time to; a request made before the wait ends it as
    # well, but a request made before the parse would be delivered elsewhere
    if (Waiting.waitForZero(CaseTimeout)) {
        printf("%s: the waiting thread did not start\n", case_name);
        flush();
        exit(1);
    }
    usleep(200ms);
    if (interrupt) {
        sm.requestInterrupt();
    } else {
        cancel_thread(tid, "test");
    }
    bool released = !waited.waitForZero(10s);
    Release.dec();
    check_done(done, case_name);
    while (Results.size()) {
        printf("%s: %s\n", case_name, Results.get());
    }
    if (waited.waitForZero(CaseTimeout)) {
        printf("%s: the waiting thread did not end\n", case_name);
        flush();
        exit(1);
    }
    printf("%s: %s\n", case_name, released ? result : "the wait did not end");
}

self_case();
chain_case();
end_wait_case("cancel", False);
end_wait_case("interrupt", True);
cross_case();
cycle3_case();
