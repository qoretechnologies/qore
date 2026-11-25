%modern
%allow-debugger
%disable-warning return-value-ignored

# the debugger events of the constructors and destructors of objects (qore#7336): a debugger records the function of
# each function entry and exit event, with the call stack of the code that the event is for

string sub frames(list<auto> cs) {
    list<string> l = ();
    foreach hash<auto> f in (cs) {
        l += sprintf("%s:%s:%d", f.function, f.type, f.line);
    }
    return l.join(" < ");
}

class Recorder inherits DebugProgram {
    public {
        list<string> events = ();
    }

    onAttach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugStep;
    }

    onDetach(ProgramControl pgm, reference rs, reference rts) {
    }

    onStep(ProgramControl pgm, int block, *int sid, *int bid, reference flow, reference rs, reference rts) {
        rs = DebugStep;
    }

    onFunctionEnter(ProgramControl pgm, int sid, reference rs, reference rts) {
        record("enter");
        rs = DebugStep;
    }

    onFunctionExit(ProgramControl pgm, int sid, reference value, reference rs, reference rts) {
        record("exit");
        rs = DebugStep;
    }

    onException(ProgramControl pgm, int sid, hash<auto> ex, reference dismiss, reference rs, reference rts) {
        events += sprintf("exception %s", ex.err);
        rs = DebugStep;
    }

    onExit(ProgramControl pgm, int sid, reference value, reference rs, reference rts) {
        rs = DebugStep;
    }

    private record(string event) {
        # the frames of the debuggee: the frames below the frames of this method and of the event method
        list<auto> cs = get_thread_call_stack();
        int i = 0;
        while (i < cs.size() && cs[i].function !~ /^(Recorder::)?on(FunctionEnter|FunctionExit)$/) {
            ++i;
        }
        events += sprintf("%s %s", event, frames(cs[(i + 1)..]));
    }
}

class Base inherits Socket {
    constructor() {
    }

    destructor() {
    }
}

class Sub inherits Base {
    public {
        int n;
    }

    constructor(int n) {
        self.n = n;
    }
}

class Thrower {
    constructor(int i) {
        if (i) {
            throw "THROWER", "constructor";
        }
    }
}

sub work() {
    Sub s(1);
    delete s;
    Socket sock();
    delete sock;
    try {
        Thrower t(1);
    } catch (hash<ExceptionInfo> ex) {
    }
}

ProgramControl pc = ProgramControl::getProgram();
for (int i = 0; i < 2; ++i) {
    Recorder recorder();
    Counter done(1);
    background sub () {
        on_exit done.dec();
        recorder.addProgram(pc);
        on_exit recorder.removeProgram(pc);
        call_function("work");
    }();
    done.waitForZero();
    printf("run %d:\n%s\n", i, recorder.events.join("\n"));
}
