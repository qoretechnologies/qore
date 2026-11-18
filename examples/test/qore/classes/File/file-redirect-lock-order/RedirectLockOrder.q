#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the script of file-redirect-lock-order.qtest: two threads redirect two Files to each other at the same time, in
# opposite directions.  With a DEBUG build of the library, the first redirect to take the first of its two locks
# waits there until the outcome is known (dbg_file_redirect_rendezvous()): the other redirect arrives there too,
# holding the other File's lock, and the two then deadlock, or it waits for the lock that the first one holds, and
# the redirects end one after the other.
#
# usage: RedirectLockOrder.q
#
# output: RESULT:<"ok", or "deadlock"> [RENDEZVOUS:<arrived> <timed out> <outcome>]

%modern

#! the safety net of the rendezvous: the outcome is known as soon as the second redirect locks or waits
const RendezvousMs = 5000;

File a();
a.open2("/dev/null", O_WRONLY);
File b();
b.open2("/dev/null", O_WRONLY);

bool hook = Option::HAVE_LIBRARY_DEBUGGING;
if (hook) {
    call_function("dbg_file_redirect_rendezvous", 2, RendezvousMs);
}

Counter running(2);
background sub () {
    on_exit running.dec();
    a.redirect(b);
}();
background sub () {
    on_exit running.dec();
    b.redirect(a);
}();

# both redirects holding a lock of their own at the rendezvous deadlock once it releases them; the threads hold the
# locks of the Files, so exit() ends the process without waiting for them
if (hook && call_function("dbg_file_redirect_rendezvous_wait", RendezvousMs) == "arrived") {
    stdout.printf("RESULT:deadlock\n");
    exit(1);
}
# a safety net: the redirects end at once otherwise
if (running.waitForZero(RendezvousMs + 30000)) {
    stdout.printf("RESULT:deadlock\n");
    exit(1);
}
string out = "RESULT:ok";
if (hook) {
    hash<auto> r = call_function("dbg_file_redirect_rendezvous_result");
    out += sprintf(" RENDEZVOUS:%d %d %s", r.arrived, r.timed_out, r.outcome);
}
stdout.printf("%s\n", out);
