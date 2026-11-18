%modern
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the cleanup of a foreach statement runs when the check for cancellation on entering an iteration raises (issue #5641)
class It inherits AbstractIterator {
    public { int i = 0; Queue log; }
    constructor(Queue l) { log = l; }
    destructor() { log.push("iterator released"); }
    bool next() { return ++i <= 3; }
    auto getValue() { return i; }
    bool valid() { return i > 0 && i <= 3; }
}
class Holder {
    public { Queue log; }
    constructor(Queue l) { log = l; }
    destructor() { log.push("list element released"); }
}
sub cancel_me() {
    Counter go(1); Counter cancelled(1); int tid = gettid();
    background sub () { go.waitForZero(); cancel_thread(tid, "t"); cancelled.dec(); }();
    defer_thread_cancel(sub () { go.dec(); cancelled.waitForZero(); });
}
#! runs the cases in a function of its own, which JIT and tiered execution compile before it runs
sub work(Queue log) {
    try {
        foreach int x in (new It(log)) {
            log.push("iter " + x);
            if (x == 1) { cancel_me(); }
        }
        log.push("no exception");
    } catch (hash<ExceptionInfo> ex) {
        clear_thread_cancel();
        log.push("catch: " + ex.err + ": " + (ex.desc =~ x/^([a-z-]+ loop)/)[0]);
    }
    try {
        int n = 0;
        foreach Holder h in ((new Holder(log),)) {
            log.push("list iter");
            cancel_me();
        }
    } catch (hash<ExceptionInfo> ex) {
        clear_thread_cancel();
        log.push("catch2: " + ex.err);
    }
    log.push("END");
}

Queue log();
int wtid = background sub () { work(log); }();
while (True) { string s = log.get(60s); printf("%s\n", s); if (s == "END") break; }
thread_join(wtid);
while (log.size()) { printf("after join: %s\n", log.get()); }
