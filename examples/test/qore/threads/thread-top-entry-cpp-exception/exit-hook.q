#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# child process of thread-top-entry-cpp-exception.qtest (#6233): the first exit hook's entry into its Program fails
# with a C++ exception from the Program's thread entry handler; the process reports it, runs the other hook and
# ends normally

%new-style
%require-types
%strict-args

Program p(PO_NEW_STYLE);
p.parse("%new-style
sub register() {
    # its entry into this Program fails, so it does not run
    register_exit_hook(sub () { printf(\"failed hook ran\\n\"); });
}
", "exit-hook");
p.setThreadEntryHandler(int sub () { return 1; }, sub (auto token) {});
p.callFunction("register");
# a hook of this Program, which has no entry handler, runs (the hooks run in the reverse order of registration)
register_exit_hook(sub () { printf("other hook ran\n"); });
# the next entry handler run is the exit hook's entry into p (QDPES_ENTRY_HANDLER)
call_function("dbg_fail_next_program_entry", 6);
printf("main done\n");
