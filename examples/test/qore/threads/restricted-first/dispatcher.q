#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# ParallelToolDispatcher (QoreAgentStrategies) loaded first by a Program that may not control threads (qorus#867): its
# threads start, governed by the caller's sandbox (its thread limit); this Program does not load the module itself

# Copyright 2026 Qore Technologies, s.r.o.

%new-style
%require-types
%strict-args

const Source = "
# dispatches 2 tool calls in parallel; returns the result or the error of each.  The first tool's thread runs until the
# second start was tried: the second tool releases it when its thread starts, and the refusal of the second start
# releases it otherwise, so a sandbox limit of one thread is always reached (the wait is bounded only against a hang)
list<string> sub run(int n) {
    QoreAgentStrategies::ParallelToolDispatcher d({\"max_concurrent\": n});
    list<hash<auto>> tools = ();
    for (int i = 0; i < n; ++i) {
        tools += {\"name\": \"t\" + i};
    }
    Counter second_tried(1);
    try {
        list<hash<auto>> results = d.dispatchToolsParallel(tools, auto sub (string name, hash<auto> args) {
            if (name == \"t0\") {
                second_tried.waitForZero(60s);
            } else if (name == \"t1\") {
                second_tried.dec();
            }
            return name;
        });
        return map $1.error ?? \"ok\", results;
    } catch (hash<ExceptionInfo> ex) {
        second_tried.dec();
        return (ex.err,);
    }
}
";

# a restricted Program that loads the module first, with a sandbox allowing max_threads threads
Program sub restricted(int max_threads) {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_NO_THREAD_CONTROL);
    p.loadModule("QoreAgentStrategies");
    p.parse(Source, "restricted");
    SandboxManager sm();
    sm.setMaxThreads(max_threads);
    p.setSandboxManager(sm);
    return p;
}

# the Programs are kept until the end, so that the threads of their objects end first
Program limited = restricted(1);
Program free = restricted(5);
printf("limited: %s\n", (foldl $1 + "," + $2, limited.callFunction("run", 2)));
printf("started: %s\n", (foldl $1 + "," + $2, free.callFunction("run", 2)));
