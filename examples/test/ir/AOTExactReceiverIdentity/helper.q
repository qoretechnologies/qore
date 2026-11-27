#!/usr/bin/env qore
# Copyright (c) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
%modern

# compiled with thresholds that outline the try blocks of identityHelper() into helpers (see
# AOTExactReceiverIdentity.qtest): the entry cleanup of the receiver is released before the call of a helper that
# assigns it (see QoreIRToLLVM::recordHelperSite()), so the first helper frees the receiver of the entry, and the
# object that the second one creates can be given its address; the call after them runs the method of the new
# receiver (qore#7631)

class IdentityBase {
    int next(int value) { return value + 1; }
}
class IdentityDerived inherits IdentityBase {
    int next(int value) { return value + 10; }
}
class IdentityThird inherits IdentityBase {
    int next(int value) { return value + 100; }
}

int sub identityHelper(IdentityBase receiver, int n) {
    int first = receiver.next(0);
    try {
        receiver = new IdentityThird();
        if (n > 5) {
            throw "IDENTITY-REPLACED", "test";
        }
    } catch (hash<ExceptionInfo> ex) {
        first += 1000;
    }
    try {
        receiver = new IdentityDerived();
        if (n > 6) {
            throw "IDENTITY-REPLACED", "test";
        }
    } catch (hash<ExceptionInfo> ex) {
        first += 2000;
    }
    return first * 100 + receiver.next(0);
}

# the receivers are temporary arguments, so that the helpers release the last reference to them
list<int> results;
for (int i = 0; i < 12; ++i) {
    push results, identityHelper(new IdentityBase(), i % 9);
}
printf("%s\nexact-receiver-helper: ok\n", results.join(":"));
