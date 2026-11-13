# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern

our hash<auto!> target;
our int destroyed = 0;
class Probe {
    destructor() {
        ++destroyed;
        throw "CLEANUP", "captured local";
    }
}
sub fill() {
    auto local = new Probe();
    target = {"a": \local};
}
fill();
bool caught = False;
try {
    # The reference in this hash is the captured local's last owner. Plain assignment releases it.
    target = {};
} catch (hash<ExceptionInfo> e) {
    if (e.err != "CLEANUP") {
        rethrow;
    }
    caught = True;
}
if (!caught || destroyed != 1 || target.size() != 0) {
    throw "TEST-FAIL", sprintf("caught=%y, destroyed=%d, target=%y", caught, destroyed, target);
}
printf("PASS captured reference exception\n");
