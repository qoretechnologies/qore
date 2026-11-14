# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
%requires ctorprobe
class Argument { destructor() { throw "ARGUMENT"; } }
sub builtinCleanup() { auto product = new CtorProbe::Builtin(new Argument(), False); }
sub externalCleanup() { auto product = new CtorProbe::External(new Argument(), False); }
sub builtinFailure() { auto product = new CtorProbe::Builtin(1, True); }
sub externalFailure() { auto product = new CtorProbe::External(1, True); }
sub builtinSuccess() { auto product = new CtorProbe::Builtin(1, False); }
sub externalSuccess() { auto product = new CtorProbe::External(1, False); }
sub run(string name, code callback) {
    try { callback(); }
    catch (hash<ExceptionInfo> ex) { printf("%s: %s\n", name, ex.err); }
    printf("%s: %y\n", name, CtorProbe::counts());
}
for (int i = 0; i < 3; ++i) {
    run("builtin-cleanup", \builtinCleanup());
    run("external-cleanup", \externalCleanup());
    run("builtin-failure", \builtinFailure());
    run("external-failure", \externalFailure());
    run("builtin-success", \builtinSuccess());
    run("external-success", \externalSuccess());
}
