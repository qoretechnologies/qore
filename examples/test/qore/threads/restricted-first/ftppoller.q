#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# FtpPoller loaded first by a Program that may not control threads (qorus#867): its thread starts, governed by the
# caller's sandbox (its thread limit); this Program does not load the module itself

# Copyright 2026 Qore Technologies, s.r.o.

%new-style
%require-types
%strict-args

# the FTP server needs these; the restricted Program loads FtpPoller itself
%requires Util
%requires FsUtil
%include ../../classes/FtpServer.qc

const Source = "
class RestrictedPoller inherits FtpPoller::FtpPoller {
    constructor(FtpClient c, hash<auto> opts) : FtpPoller::FtpPoller(c, opts) {}
    singleFileEvent(hash<FtpPoller::FtpPollerFileEventInfo> event) {}
    postSingleFileEvent(hash<FtpPoller::FtpPollerFileEventInfo> event) {}
}
# starts n pollers; returns the result of each start
list<string> sub run(string dir, int port, int n) {
    list<RestrictedPoller> pollers = ();
    list<string> rv = ();
    on_exit map $1.stop(), pollers;
    for (int i = 0; i < n; ++i) {
        RestrictedPoller fp(new FtpClient(\"ftp://user:pass@127.0.0.1:\" + port), {\"path\": dir,
            \"sleep\": sub (softint s) {}});
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
    p.loadModule("FtpPoller");
    p.parse(Source, "restricted");
    SandboxManager sm();
    sm.setMaxThreads(max_threads);
    # the poller connects to the local FTP server
    sm.addNetworkAllowedIPRange("127.0.0.0/8");
    p.setSandboxManager(sm);
    return p;
}

string dir = ARGV[0];
FtpServer serv(0, -1, "127.0.0.1");
on_exit serv.shutdown();
serv.setPwd(dir);
int port = serv.getPort();
# the Programs are kept until the end, so that the threads of their objects end first
Program limited = restricted(1);
Program free = restricted(5);
printf("limited: %s\n", (foldl $1 + "," + $2, limited.callFunction("run", dir, port, 2)));
printf("started: %s\n", (foldl $1 + "," + $2, free.callFunction("run", dir, port, 2)));
