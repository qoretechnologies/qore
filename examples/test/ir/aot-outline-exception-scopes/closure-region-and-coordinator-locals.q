# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# a closure created in an outlined helper captures a local of the region and locals of the coordinator; the
# helper's scope exits pop its own closure variables (qore#6654); see AOTOutlineExceptionScopes.qtest

%modern
sub t() {
    int n = 0;
    Mutex m();
    list<code> l = ();
    for (int round = 0; round < 3; ++round) {
        Counter done(2);
        for (int i = 0; i < 2; ++i) {
            l += sub () { m.lock(); on_exit m.unlock(); ++n; done.dec(); };
        }
    }
    map $1(), l;
    printf("%d\n", n);
}
t();
