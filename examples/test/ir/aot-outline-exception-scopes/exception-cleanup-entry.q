# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# a region does not start at the cleanup block of an exception path, which the exception enters without a landing
# pad: a catch block in a handler then still has its exception (qore#6651); see AOTOutlineExceptionScopes.qtest

%modern
our list<string> log = ();
sub t1() {
    try { throw "OUTER"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
        try {
            try { throw "A"; } catch (hash<ExceptionInfo> ex) { thread_exit; }
        } catch (hash<ExceptionInfo> e) { log += "caught:" + e.err; }
        log += "not reached";
    }
}
Counter c(1);
background sub () { on_exit c.dec(); t1(); }();
c.waitForZero();
printf("t1: %y\n", log);
