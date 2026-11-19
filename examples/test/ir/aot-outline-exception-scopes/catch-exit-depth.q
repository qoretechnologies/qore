# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the exit block of a catch block in an outlined helper closes the catch blocks opened since its try statement
# began, so that a rethrow in a later catch block of the helper still has its exception (qore#6651); see
# AOTOutlineExceptionScopes.qtest

%modern
our list<string> log = ();
sub s2() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        try {
            {
                on_exit { throw "H"; }
                try { throw "A"; } catch () { return; }
            }
        } catch (hash<ExceptionInfo> h) {
            log += "h:" + h.err;
        }
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "o:" + r.err; }
    }
}
try { s2(); } catch (hash<ExceptionInfo> e) { log += "EXC:" + e.err + ":" + e.desc; }
printf("%y\n", log);
