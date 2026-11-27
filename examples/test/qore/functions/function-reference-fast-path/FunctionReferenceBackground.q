#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The expression of the background operator calls, through a reference, a function of a Program that may not control
# threads (qore#7087, qore#5578): the call is refused in the new thread, in every execution mode; the refusal ends the
# thread with an exception on stderr.  Calls of the reference that a thread makes itself run.  The fast call through
# a function reference ran the function in the IR, JIT and tiered modes when it entered other Programs than modules'
# (it takes the generic call path for them now, and refuses such a call of a module's code itself)

%modern
%no-child-restrictions

Program b(PO_NEW_STYLE | PO_NO_THREAD_CONTROL);
b.parse("%modern
sub g(Counter done, int unused) {
    done.dec();
}
sub export_to(Program p) {
    p.importFunction(\"g\");
}
", "restricted");

Program a(PO_NEW_STYLE);
b.callFunction("export_to", a);
a.parse("%modern
sub dec_end(Counter end) {
    end.dec();
}
# called by the new thread for an argument of the call: the counter is decremented when the thread ends
int sub signal_end(Counter end) {
    set_thread_resource(\\dec_end(), end);
    return 1;
}
code sub get_ref() {
    return \\g();
}
string sub run(int n) {
    code r = get_ref();
    for (int i = 0; i < n; ++i) {
        Counter c(1);
        r(c, i);
        if (c.getCount()) {
            return \"not run in this thread\";
        }
    }
    Counter done(1);
    Counter end(1);
    background r(done, signal_end(end));
    if (end.waitForZero(60s)) {
        return \"timeout\";
    }
    return done.getCount() ? \"not run\" : \"ran\";
}
", "caller");

printf("background call through a reference to restricted code: %s\n", a.callFunction("run", 200));
