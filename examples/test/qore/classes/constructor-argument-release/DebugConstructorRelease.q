# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
%allow-debugger

class Recorder inherits DebugProgram {
    public { int exceptions; }
    onAttach(ProgramControl pgm, reference rs, reference rts) { rs = DebugStep; }
    onDetach(ProgramControl pgm, reference rs, reference rts) {}
    onStep(ProgramControl pgm, int block, *int sid, *int bid, reference flow, reference rs, reference rts) {
        rs = DebugStep;
    }
    onFunctionEnter(ProgramControl pgm, int sid, reference rs, reference rts) { rs = DebugStep; }
    onFunctionExit(ProgramControl pgm, int sid, reference value, reference rs, reference rts) { rs = DebugStep; }
    onException(ProgramControl pgm, int sid, hash<auto> ex, reference dismiss, reference rs, reference rts) {
        if (ex.err == "ARGUMENT") ++exceptions;
        rs = DebugStep;
    }
    onExit(ProgramControl pgm, int sid, reference value, reference rs, reference rts) { rs = DebugStep; }
}
class Arg { destructor() { printf("argument\n"); throw "ARGUMENT"; } }
class Default {
    constructor(any arg = new Arg()) { printf("completed\n"); }
    destructor() { printf("product\n"); }
}
class Extra {
    constructor(...) { argv = (new Arg(),); printf("completed\n"); }
    destructor() { printf("product\n"); }
}
class Named {
    constructor(list<auto> arg) { arg = (new Arg(),); printf("completed\n"); }
    destructor() { printf("product\n"); }
}
sub defaultCase() { auto p = new Default(); }
sub extraCase() { auto p = new Extra(1); }
sub namedCase() { auto p = new Named((1,)); }
sub cases() {
    foreach string name in ("defaultCase", "extraCase", "namedCase") {
        try { call_function(name); }
        catch (hash<ExceptionInfo> ex) { printf("caught:%s\n", ex.err); }
    }
}
Recorder recorder();
ProgramControl pc = ProgramControl::getProgram();
Counter done(1);
int tid = background sub () {
    on_exit done.dec();
    recorder.addProgram(pc);
    on_exit recorder.removeProgram(pc);
    call_function("cases");
}();
done.waitForZero();
thread_join(tid);
if (recorder.exceptions < 3) throw "MISSING-EVENTS", "debugger did not observe constructor cleanup exceptions";
printf("debugger:PASS\n");
