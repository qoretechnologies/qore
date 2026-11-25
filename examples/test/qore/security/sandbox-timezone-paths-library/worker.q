#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# loads the library module TzLibInit from sandboxed code: its initialization runs with the sandbox of that code
%new-style
%strict-args
string allowed = ARGV[0];
SandboxManager sm();
sm.setFilesystemDefaultPolicy(False);
sm.addFilesystemAllowedPath(allowed, QSEC_READ);
if (ARGV[1] == "budget") {
    sm.setTimeZonePathLimits(10, 50 * 1024 * 1024);
}
Program p(PO_NEW_STYLE);
p.setSandboxManager(sm);
p.parse("*string sub f() { load_module(\"TzLibInit\"); return call_function(\"TzLibInit::get\"); }", "tenant");
printf("init=%s zones=%d\n", p.callFunction("f"), sm.getTimeZonePathLimits().zones);
