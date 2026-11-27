#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The date literals of a sandboxed Program whose %set-time-zone directive names a zoneinfo file path are in that zone,
# also after the host has changed the Program's zone: every execution mode must print the output of the AST
# interpreter (see sandbox-timezone-path-literals.qtest)
#
# usage: sandbox-timezone-path-literals.q <zoneinfo file>

%modern
%no-child-restrictions

string zone = ARGV[0];

Program p();
SandboxManager sm();
sm.setFilesystemDefaultPolicy(False);
sm.addFilesystemAllowedPath(dirname(zone), QSEC_READ);
sm.setTimeZonePathLimits(10, 50000000);
p.setSandboxManager(sm);
# the code is %modern, so it runs in the execution mode of this program
p.parse("%modern\n%set-time-zone " + zone + "\n"
    "date sub ret() { return 2038-01-15T12:00:00; }\n"
    "date sub expr(int i) { date d = 2038-07-15T12:00:00; d += seconds(i); return d; }\n", "literals");

# a function called through Program::callFunction() runs on the IR tier in its first call and is compiled when it
# returns (see ExecModeHarness), so the later calls run native code
sub show(Program p, string when) {
    for (int i = 0; i < 3; ++i) {
        printf("%s %d: %s %s\n", when, i, p.callFunction("ret").format("IF"), p.callFunction("expr", 0).format("IF"));
    }
}

show(p, "own zone");
# the host changes the Program's zone; the literals keep the zone that they were parsed in
p.setTimeZoneRegion("Europe/Prague");
show(p, "changed zone");
