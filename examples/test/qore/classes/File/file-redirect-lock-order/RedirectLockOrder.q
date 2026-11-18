#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the script of file-redirect-lock-order.qtest: two threads redirect two Files to each other at the same time, in
# opposite directions; with a DEBUG build of the library, each redirect waits with the first of its two locks held
# until the other one has taken its first lock too, or until the timeout (dbg_file_redirect_rendezvous())
#
# usage: RedirectLockOrder.q
#
# output: RESULT:<"ok", or "deadlock" if the redirects did not end> [RENDEZVOUS:<arrived> <timed out>]

%modern

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

# the redirects end after the rendezvous at the latest
if (running.waitForZero(RendezvousMs + 30000)) {
    stdout.printf("RESULT:deadlock\n");
    stdout.sync();
    # the threads hold the locks of the Files: exit() does not wait for them while they run
    exit(1);
}
string out = "RESULT:ok";
if (hook) {
    hash<auto> r = call_function("dbg_file_redirect_rendezvous_result");
    out += sprintf(" RENDEZVOUS:%d %d", r.arrived, r.timed_out);
}
stdout.printf("%s\n", out);
