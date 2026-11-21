#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the checks of caller-restriction-grants.qtest that need a library module that restricted code loads first; prints one
# line

%modern
%no-child-restrictions

# restricted from external information; it loads the module first, so the module is initialized when it is loaded
Program p(PO_NEW_STYLE | PO_NO_EXTERNAL_INFO);
p.parse("%modern
%requires GrantProbe
list<auto> sub run() {
    return (GrantProbe::init_restricted(), GrantProbe::plain(), GrantProbe::barrier(), GrantProbe::capability(),
        GrantProbe::call_barrier(bool sub () { return SandboxManager::isCallerRestricted(DOM_EXTERNAL_INFO); }),
        GrantProbe::declared_plain(), GrantProbe::declared_barrier());
}
", "restricted");
printf("%y\n", p.callFunction("run"));
