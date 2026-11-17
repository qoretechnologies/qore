#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# child process of exit-finishing-thread.qtest (#6405): exit() is called while a background thread that has released
# its TID is held before it leaves the process's background thread count

%new-style
%require-types
%strict-args

# the thread is held for up to 3 seconds after it has released its TID
call_function("dbg_hold_next_thread_finish", 3000);
Counter c(1);
background sub () { c.dec(); }();
c.waitForZero();
if (!call_function("dbg_wait_thread_finish_held", 30000)) {
    printf("the thread was not held\n");
    exit(2);
}
printf("exiting\n");
exit(0);
