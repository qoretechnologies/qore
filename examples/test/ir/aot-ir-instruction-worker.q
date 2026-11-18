# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: LGPL-2.1-or-later

%modern
%allow-debugger
%disable-warning return-value-ignored

class InstructionRecorder inherits DebugProgram {
    public { int exceptions; }
    onAttach(ProgramControl pgm, reference rs, reference rts) { rs = DebugStep; }
    onDetach(ProgramControl pgm, reference rs, reference rts) {}
    onStep(ProgramControl pgm, int block, *int sid, *int bid, reference flow, reference rs, reference rts) {
        rs = DebugStep;
    }
    onFunctionEnter(ProgramControl pgm, int sid, reference rs, reference rts) { rs = DebugStep; }
    onFunctionExit(ProgramControl pgm, int sid, reference value, reference rs, reference rts) { rs = DebugStep; }
    onException(ProgramControl pgm, int sid, hash<auto> ex, reference dismiss, reference rs, reference rts) {
        ++exceptions;
        rs = DebugStep;
    }
    onExit(ProgramControl pgm, int sid, reference value, reference rs, reference rts) { rs = DebugStep; }
}

string sub instructionForms(auto value, int number, list<int> values) {
    list<string> result = (string(value), string(int(value)), string(float(value)), string(number));
    result += sprintf("%03d", number);
    result += join(",", (map sprintf("%03d", $1), values));
    list<int> iterated = iterate number;
    result += string(elements iterated);
    result += value instanceof string ? "string" : "other";
    list<int> selected = values[(0..1, 2)];
    result += join(",", map string($1), selected);
    hash<auto> keyed = {"a": 1, "b": 2};
    result += string(keyed.a + keyed.b);
    return join("|", result);
}

bool debugging = ARGV[0] == "debug";
InstructionRecorder recorder();
ProgramControl pc = ProgramControl::getProgram();
Counter done(1);
string result;
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
    result = call_function("instructionForms", "12", 12, (1, 2, 3));
}();
done.waitForZero();
printf("%s exceptions=%d\n", result, recorder.exceptions);
