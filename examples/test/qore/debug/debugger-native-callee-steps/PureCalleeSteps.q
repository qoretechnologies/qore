%modern
%allow-debugger
# Copyright (C) 2026 Qore Technologies, s.r.o.
int sub probe(int value) {
    int scaled = (value + 1) * 2;
    return scaled + 10;
}

class PureMethod {
    static int compute(int value) {
        int scaled = (value + 1) * 2;
        return scaled + 10;
    }
}

class Recorder inherits DebugProgram {
    public { list<int> lines = (); }
    onAttach(ProgramControl p, reference rs, reference rts) { rs = DebugStep; }
    onDetach(ProgramControl p, reference rs, reference rts) {}
    onStep(ProgramControl p, int b, *int s, *int bp, reference flow, reference rs, reference rts) {
        rs = DebugStep;
        if (s) {
            hash<auto> pos = p.getStatementIdInfo(s);
            int line = pos.start_line + pos.offset;
            if (line == 5 || line == 6 || line == 11 || line == 12 || line == 39 || line == 40 || line == 51 || line == 52) { lines += line; }
        }
    }
    onFunctionEnter(ProgramControl p, int s, reference rs, reference rts) { rs = DebugStep; }
    onFunctionExit(ProgramControl p, int s, reference result, reference rs, reference rts) { rs = DebugStep; }
    onException(ProgramControl p, int s, hash<auto> ex, reference dismiss, reference rs, reference rts) {}
    onExit(ProgramControl p, int s, reference result, reference rs, reference rts) {}
}

int sub runProbe(Recorder recorder) {
    recorder.addProgram(ProgramControl::getProgram());
    int result = probe(3);
    result += PureMethod::compute(3);
    code<int(int)> calc = int sub (int value) {
        int scaled = (value + 1) * 2;
        return scaled + 10;
    };
    result += calc(3);
    recorder.removeProgram(ProgramControl::getProgram());
    return result;
}

int sub runOutlined(Recorder recorder) {
    recorder.addProgram(ProgramControl::getProgram());
    list out;
    out += map (int sub () {
        int item = 7;
        return item * 2;
    })(), (1, 2);
    foreach auto val in (out) {
    }
    recorder.removeProgram(ProgramControl::getProgram());
    return out[0] + out[1];
}

Recorder recorder();
int result = ARGV[0] == "outlined" ? runOutlined(recorder) : runProbe(recorder);
printf("%d %y\n", result, recorder.lines);
