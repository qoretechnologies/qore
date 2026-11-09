%modern
# Copyright (C) 2026 Qore Technologies, s.r.o.
# an exception raised inside an expression reaches the catch block of the try block that the expression is in
# (issue #5641): a cancellation that the expression does not raise, and an error of its runtime helper
sub cancel_me() {
    Counter go(1); Counter cancelled(1); int tid = gettid();
    background sub () { go.waitForZero(); cancel_thread(tid, "t"); cancelled.dec(); }();
    defer_thread_cancel(sub () { go.dec(); cancelled.waitForZero(); });
}
Queue q();
background sub () {
    list<int> li = range(300);
    cancel_me();
    try {
        list<int> y = (map $1, li);
        q.push("map: ok " + y.size());
    } catch (hash<ExceptionInfo> ex) {
        q.push("map: caught " + ex.err + ": " + ex.desc);
    }
    clear_thread_cancel();
    list<string> key_list = ("a", "b\x00c");
    try {
        hash<auto> h = map {$1: True}, key_list;
        q.push("hmap: no exception " + h.size());
    } catch (hash<ExceptionInfo> ex) {
        q.push("hmap: caught " + ex.err);
    }
    try {
        hash<auto> h = map {$1: True}, key_list, $1 != "x";
        q.push("hmap-select: no exception " + h.size());
    } catch (hash<ExceptionInfo> ex) {
        q.push("hmap-select: caught " + ex.err);
    }
    q.push("END");
}();
while (True) { string s = q.get(60s); printf("%s\n", s); if (s == "END") break; }
