# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: LGPL-2.1-or-later

%modern
%allow-debugger
%disable-warning return-value-ignored

class ExitRecorder inherits DebugProgram {
    public {
        int exceptions;
        int unexpected;
    }
    onAttach(ProgramControl pgm, reference rs, reference rts) { rs = DebugStep; }
    onDetach(ProgramControl pgm, reference rs, reference rts) {}
    onStep(ProgramControl pgm, int block, *int sid, *int bid, reference flow, reference rs, reference rts) {
        rs = DebugStep;
    }
    onFunctionEnter(ProgramControl pgm, int sid, reference rs, reference rts) { rs = DebugStep; }
    onFunctionExit(ProgramControl pgm, int sid, reference value, reference rs, reference rts) { rs = DebugStep; }
    onException(ProgramControl pgm, int sid, hash<auto> ex, reference dismiss, reference rs, reference rts) {
        ++exceptions;
        if (ex.err != "EXPECTED") {
            ++unexpected;
        }
        rs = DebugStep;
    }
    onExit(ProgramControl pgm, int sid, reference value, reference rs, reference rts) { rs = DebugStep; }
}

class ExitInDestructor {
    destructor() { thread_exit; }
}

class ExitProbe {
    public {
        int cleanup;
        int continued;
        int caught;
    }

    exitNow() { thread_exit; }

    runCase(string which) {
        on_exit ++cleanup;
        switch (which) {
            case "direct": thread_exit;
            case "nested": call_object_method(self, "exitNow"); break;
            case "caught":
                try {
                    call_object_method(self, "exitNow");
                } catch (hash<auto> ex) {
                    ++caught;
                }
                break;
            case "destructor": {
                ExitInDestructor value();
                break;
            }
            case "exception":
                try {
                    throw "EXPECTED", "real exception control";
                } catch (hash<auto> ex) {
                    ++caught;
                }
                break;
        }
        ++continued;
    }
}

bool debugging = ARGV[0] == "debug";
foreach string which in (("direct", "nested", "caught", "destructor", "exception")) {
    ExitRecorder recorder();
    ExitProbe probe();
    ProgramControl pc = ProgramControl::getProgram();
    Counter done(1);
    background sub () {
        on_exit done.dec();
        if (debugging) {
            recorder.addProgram(pc);
        }
        on_exit {
            if (debugging) {
                recorder.removeProgram(pc);
            }
        }
        # Enter after debugger attachment, including AOT materialization of the method's debug IR.
        call_object_method(probe, "runCase", which);
    }();
    done.waitForZero();
    printf("%s cleanup=%d continued=%d caught=%d exception=%y unexpected=%d\n", which,
        probe.cleanup, probe.continued, probe.caught, recorder.exceptions > 0, recorder.unexpected);
}
