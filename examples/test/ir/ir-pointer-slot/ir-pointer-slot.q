# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a remove whose second selector raises: the IR interpreter sets the exception aside (ExceptionSuspend) while the first
# selector removes its element, which is removed (issue #6345: on musl the handle's address was refused and nothing
# was removed); run by ir-pointer-slot.qtest
%modern

int sub raise_selector() {
    throw "SEL-ERROR";
}

list<int> l = (10, 11, 12, 13);
try {
    remove l[0, raise_selector()];
} catch (hash<ExceptionInfo> ex) {
    # the element of the selector before the one that raised is removed
    printf("caught %s: ", ex.err);
}
printf("%y\n", l);
