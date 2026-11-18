#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

# a foreign thread (registered with q_register_foreign_thread()) calls module code, which saves an object in the
# thread's data in the module's Program; the destructor of the object runs module code when the thread is
# deregistered, which deletes its data; module-code-cleanup.qtest checks the output

%modern
%requires CleanupMod

for (int i = 0; i < 3; ++i) {
    *string err = call_function("dbg_run_in_foreign_thread", sub () {
        int s = 0;
        for (int j = 0; j < 100; ++j) {
            s = CleanupMod::f(s);
        }
        if (s != 100) {
            throw "FOREIGN-ERROR", sprintf("s: %d", s);
        }
        CleanupMod::keep();
    });
    printf("foreign thread %d: %y\n", i, err);
}
printf("destructor calls in the module's Program at deregistration: %d\n", CleanupMod::holder_calls());
