# -*- mode: qore; indent-tabs-mode: nil -*-
# module-exit-shared.q - script of static-var-release.qtest: a user module's static variables are released at
# teardown while a Program created by a destructor shares classes whose variables have already been released

# Copyright 2026 Qore Technologies, s.r.o.

%modern
%requires StaticVarReleaseShared

printf("main done: %d\n", Holder::value());
