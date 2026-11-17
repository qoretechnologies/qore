#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# child process of exit-finishing-thread.qtest (#6412): exit() is called while a background thread is in a
# thread-local destructor, after its thread function has returned

%new-style
%require-types
%strict-args

# the thread is held for up to 30 seconds in a thread-local destructor
call_function("dbg_hold_next_thread_tls_exit", 30000);
Counter c(1);
background sub () { c.dec(); }();
c.waitForZero();
if (!call_function("dbg_wait_thread_finish_held", 30000)) {
    printf("the thread was not held\n");
    exit(2);
}
printf("exiting\n");
exit(0);
