# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# a private:internal method called from an outlined helper is the one that the parser bound, not a base class's
# static method of the same name (qore#6653); see AOTOutlineExceptionScopes.qtest

%modern
class KP {
    static x(string v) { printf("KP::x static %s\n", v); }
}
final class KC inherits KP {
    private:internal x(int v) { printf("KC::x normal %d\n", v); }
    run() { (sub () { self.x(1); })(); self.x(2); }
}
KC kc();
kc.run();
