# -*- mode: qore; indent-tabs-mode: nil -*-
# module-exit.q - script of static-var-release.qtest: a user module's static variable is released at teardown

# Copyright 2026 Qore Technologies, s.r.o.

%modern
%requires StaticVarReleaseGuard

printf("main done: %d\n", Holder::value());
