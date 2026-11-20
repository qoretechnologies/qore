%modern
%allow-debugger
%enable-debug

final class Receiver {
    private string pick(int value) { return "instance int"; }
    private string pick(string value) {
        if (value == "throw") { throw "INSTANCE-PICK", "instance pick"; }
        return "instance string";
    }
    static string pick(int value) { return "static int"; }
    static string pick(string value) {
        if (value == "throw") { throw "STATIC-PICK", "static pick"; }
        return "static string";
    }

    public { static string Saved = (new Receiver()).pick(1); }
    string guarded(auto value) {
        return (string sub () {
            try { return self.pick(value); }
            catch (hash<ExceptionInfo> ex) { return ex.err; }
        })();
    }
    string inside(auto value) {
        return (string sub () { return self.pick(value); })();
    }
}
class Caller {
    public { static string Saved = (new Receiver()).pick(1); }
    string guarded(Receiver obj, auto value) {
        return (string sub () {
            try { return obj.pick(value); }
            catch (hash<ExceptionInfo> ex) { return ex.err; }
        })();
    }
    string outside(Receiver obj, auto value) {
        return (string sub () { return obj.pick(value); })();
    }
}
class Base {}
class Derived inherits Base {
    private:internal string lexical() { return "derived internal"; }
    string consume(Base receiver) {
        return (string sub () { return receiver.lexical(); })();
    }
}
class Stepper inherits DebugProgram {
    public { int steps; }
    onAttach(ProgramControl pgm, reference rs, reference rts) { rs = DebugStep; }
    onDetach(ProgramControl pgm, reference rs, reference rts) { rs = DebugDetach; }
    onStep(ProgramControl pgm, int blk, *int sid, *int bid, reference flow, reference rs, reference rts) {
        if (sid) { ++steps; }
        rs = DebugStep;
    }
    onFunctionEnter(ProgramControl pgm, int sid, reference rs, reference rts) { rs = DebugStep; }
    onFunctionExit(ProgramControl pgm, int sid, reference rv, reference rs, reference rts) {}
    onException(ProgramControl pgm, int sid, hash<auto> ex, reference dismiss, reference rs, reference rts) {}
    onExit(ProgramControl pgm, int sid, reference rv, reference rs, reference rts) {}
}
Receiver obj();
Caller caller();
Derived derived();
Stepper dbg();
ProgramControl pc = ProgramControl::getProgram();
Counter done(1);
background sub () {
    on_exit done.dec();
    dbg.addProgram(pc);
    on_exit dbg.removeProgram(pc);
    printf("init: %s | %s\n", Receiver::Saved, Caller::Saved);
    printf("direct instance: %s | %s\n", obj.inside(1), obj.inside("s"));
    printf("direct static: %s | %s\n", caller.outside(obj, 1), caller.outside(obj, "s"));
    printf("invoke instance: %s | %s | %s\n", obj.guarded(1), obj.guarded("s"), obj.guarded("throw"));
    printf("invoke static: %s | %s | %s\n", caller.guarded(obj, 1), caller.guarded(obj, "s"),
        caller.guarded(obj, "throw"));
    printf("%s\n", derived.consume(derived));
}();
done.waitForZero();
printf("stepped=%y\n", dbg.steps > 0);
