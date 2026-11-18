#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

# module code checks a sandboxed caller's reference to a function whose Program another thread keeps allocated while
# the Program is deleted; module-code-program-barrier.qtest runs it with a debug build in every execution mode
/*  A thread that calls a function of a Program from another Program holds a reference to the Program until the call
    has left it, as the thread that runs the reference that module code scheduled for the first call does; the debug
    build holds such a call at that point while the Program is deleted.  The Program stays allocated, but no thread
    can enter it anymore, so module code must refuse the reference as one to a function of a deleted Program
*/

%modern
%no-child-restrictions
%requires BarrierMod

const Restrictions = PO_NO_EXTERNAL_INFO | PO_NO_THREAD_CONTROL | PO_NO_THREAD_CLASSES;
const StepTimeout = 60000;

Program defining(PO_NEW_STYLE);
defining.parse("string sub defined() { return 'defined'; }
sub export_to(Program p) { p.importFunction('defined'); }", "defining");
Program user(PO_NEW_STYLE | Restrictions);
user.setSandboxManager(new SandboxManager());
defining.callFunction("export_to", user);
user.parse("%requires BarrierMod
auto sub run() { return BarrierMod::func_async(\\defined()); }
auto sub call_defined() { return defined(); }", "user");

# repeated calls run the code on every tier with tiered execution; no reference is scheduled before the hold, so the
# call held is the one made below
*string before;
for (int i = 0; i < 3; ++i) {
    before = user.callFunction("call_defined");
}
printf("before: %y\n", before);

# holds the next call of defined() from another Program once it has taken the defining Program with a reference
call_function("dbg_hold_source_program_call", defining);
*string rv;
*string err;
Counter done(1);
background sub () {
    try {
        rv = user.callFunction("call_defined");
    } catch (hash<ExceptionInfo> ex) {
        err = ex.err;
    }
    done.dec();
}();
hash<auto> h = call_function("dbg_wait_source_program_call_held", StepTimeout);
delete defining;
printf("held: %y referenced: %y\n", h.held, h.referenced);
printf("after: %y\n", user.callFunction("run"));
bool continued = call_function("dbg_continue_source_program_call", StepTimeout);
done.waitForZero();
printf("continued: %y rv: %y err: %y\n", continued, rv, err);
