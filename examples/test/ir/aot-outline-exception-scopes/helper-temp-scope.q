# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the temporaries of a statement in a try block of an outlined helper are released before the catch block when
# the try statement's temp scope is the coordinator's (qore#6650); see AOTOutlineExceptionScopes.qtest

%modern
class Tracked { public { string n; } constructor(string n) { self.n = n; } destructor() { printf("release %s\n", n); } }
string sub thrower(string s) { throw "X", s; }
sub hashMapKeyRelease() {
    printf("hash map value:\n");
    try {
        hash h = (map {$1.n: ($1.n == "w2" ? thrower("z") : 1)},
            (new Tracked("w1"), new Tracked("w2"), new Tracked("w3")));
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.desc);
    }
}
hashMapKeyRelease();
