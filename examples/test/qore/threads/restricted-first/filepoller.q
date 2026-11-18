#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# FilePoller loaded first by a Program that may not control threads (qorus#867): its thread starts, governed by the
# caller's sandbox (its thread limit); this Program does not load the module itself

# Copyright 2026 Qore Technologies, s.r.o.

%new-style
%require-types
%strict-args

const Source = "
class RestrictedPoller inherits FilePoller::FilePoller {
    constructor(string dir) : FilePoller::FilePoller(dir, \".*\", {\"poll_interval\": 1}) {}
    singleFileEvent(hash<FilePoller::FilePollerFileEventInfo> event) {}
}
# starts n pollers; returns the result of each start
list<string> sub run(string dir, int n) {
    list<RestrictedPoller> pollers = ();
    list<string> rv = ();
    on_exit map $1.stop(), pollers;
    for (int i = 0; i < n; ++i) {
        RestrictedPoller fp(dir);
        try {
            fp.start();
            pollers += fp;
            rv += \"started\";
        } catch (hash<ExceptionInfo> ex) {
            rv += ex.err;
        }
    }
    return rv;
}
";

# a restricted Program that loads the module first, with a sandbox allowing max_threads threads
Program sub restricted(int max_threads) {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_NO_THREAD_CONTROL);
    p.loadModule("FilePoller");
    p.parse(Source, "restricted");
    SandboxManager sm();
    sm.setMaxThreads(max_threads);
    p.setSandboxManager(sm);
    return p;
}

string dir = ARGV[0];
# the Programs are kept until the end, so that the threads of their objects end first
Program limited = restricted(1);
Program free = restricted(5);
# with a sandbox thread limit of 1, the second thread that the module starts for the caller is refused
printf("limited: %s\n", (foldl $1 + "," + $2, limited.callFunction("run", dir, 2)));
printf("started: %s\n", (foldl $1 + "," + $2, free.callFunction("run", dir, 2)));
