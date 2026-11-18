# Copyright (C) 2026 Qore Technologies, s.r.o.

%modern
%allow-debugger
%disable-warning return-value-ignored

class ExceptionRecorder inherits DebugProgram {
    public { list<string> events = (); }

    onAttach(ProgramControl pgm, reference rs, reference rts) { rs = DebugStep; }
    onDetach(ProgramControl pgm, reference rs, reference rts) {}
    onStep(ProgramControl pgm, int block, *int sid, *int bid, reference flow, reference rs, reference rts) {
        rs = DebugStep;
    }
    onFunctionEnter(ProgramControl pgm, int sid, reference rs, reference rts) { rs = DebugStep; }
    onFunctionExit(ProgramControl pgm, int sid, reference result, reference rs, reference rts) { rs = DebugStep; }
    onException(ProgramControl pgm, int sid, hash<auto> ex, reference dismiss, reference rs, reference rts) {
        hash<auto> loc = pgm.getStatementIdInfo(sid);
        events += sprintf("event %s %s:%d", ex.desc, basename(loc.file), loc.start_line + loc.offset);
        rs = DebugStep;
    }
    onExit(ProgramControl pgm, int sid, reference result, reference rs, reference rts) { rs = DebugStep; }
}

class Thrower {
    private { string label; }
    constructor(string label) { self.label = label; }
    destructor() { throw "DTOR", label; }
}

sub assigned() {
    string value = sprintf("%y", new Thrower("assigned")); #! assigned
    throw "MISSED", "assigned returned normally";
}

sub ignored() {
    sprintf("%y", new Thrower("ignored")); #! ignored
    throw "MISSED", "ignored returned normally";
}

string sub locallyCaught() {
    try {
        string value = sprintf("%y", new Thrower("local")); #! local
    } catch (hash<auto> ex) {
        return ex.err;
    }
    return "MISSED";
}

sub indirect() {
    code<string(string, auto)> format = \sprintf();
    string value = format("%y", new Thrower("indirect")); #! indirect
    throw "MISSED", "indirect returned normally";
}

string sub indirectCaught() {
    code<string(string, auto)> format = \sprintf();
    try {
        string value = format("%y", new Thrower("indirect-local")); #! indirect-local
    } catch (hash<auto> ex) {
        return ex.err;
    }
    return "MISSED";
}

sub plainThrow() { throw "PLAIN", "plain"; }

sub propagated() {
    call_function("plainThrow"); #! plain
}

class Receiver {
    static string accept(Thrower value) { return "accepted"; }
    string consume(Thrower value) { return "accepted"; }

    static staticArgument() {
        string value = Receiver::accept(new Thrower("static")); #! static
        throw "MISSED", "static returned normally";
    }

    selfArgument() {
        string value = consume(new Thrower("self")); #! self
        throw "MISSED", "self returned normally";
    }

    scopedArgument() {
        string value = Receiver::consume(new Thrower("scoped")); #! scoped
        throw "MISSED", "scoped returned normally";
    }
}

string sub normal() {
    return sprintf("%s", "normal");
}

sub runCases() {
    # Dynamic dispatch enters the AOT function's debug IR when the debugger is attached.
    foreach string name in (("assigned", "ignored", "indirect")) {
        try {
            call_function(name);
        } catch (hash<auto> ex) {
            printf("caught %s %s\n", name, ex.err);
        }
    }
    printf("local %s\n", call_function("locallyCaught"));
    printf("indirect-local %s\n", call_function("indirectCaught"));
    try { call_function("propagated"); } catch (hash<auto> ex) { printf("plain %s\n", ex.err); }
    try { call_static_method("Receiver", "staticArgument"); }
    catch (hash<auto> ex) { printf("static %s\n", ex.err); }
    Receiver receiver();
    foreach string name in (("selfArgument", "scopedArgument")) {
        try { call_object_method(receiver, name); }
        catch (hash<auto> ex) { printf("%s %s\n", name, ex.err); }
    }
    printf("normal %s\n", call_function("normal"));
}

ExceptionRecorder recorder();
ProgramControl pc = ProgramControl::getProgram();
Counter done(1);
background sub () {
    on_exit done.dec();
    bool debugging = ARGV[0] == "debug";
    if (debugging) {
        recorder.addProgram(pc);
    }
    on_exit {
        if (debugging) {
            recorder.removeProgram(pc);
        }
    }
    call_function("runCases");
}();
done.waitForZero();
foreach string event in (recorder.events) {
    printf("%s\n", event);
}
