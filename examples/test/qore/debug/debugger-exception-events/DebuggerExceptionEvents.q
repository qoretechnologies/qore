#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# prints the exception events that a debugger gets for programs that raise exceptions in different statements, as
# "<code>@<line>", one program per line; run by debugger-exception-events.qtest in every execution mode

%modern
%allow-debugger
%no-child-restrictions

#! records the exception events of the programs it debugs
class ExceptionRecorder inherits DebugProgram {
    public {
        list<string> seen = ();
    }

    onAttach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugRun;
    }
    onDetach(ProgramControl pgm, reference rs, reference rts) {
    }
    onStep(ProgramControl pgm, int blockStatementId, *int statementId, *int breakpointId, reference flow,
            reference rs, reference rts) {
    }
    onFunctionEnter(ProgramControl pgm, int statementId, reference rs, reference rts) {
    }
    onFunctionExit(ProgramControl pgm, int statementId, reference result, reference rs, reference rts) {
    }
    onException(ProgramControl pgm, int statementId, hash<auto> ex, reference dismiss, reference rs,
            reference rts) {
        hash<auto> i = pgm.getStatementIdInfo(statementId);
        seen += sprintf("%s@%d", ex.err, i.start_line + i.offset);
    }
    onExit(ProgramControl pgm, int statementId, reference result, reference rs, reference rts) {
    }
}

#! the programs, each of which ends with an exception or catches one
const Programs = (
    ("top-level throw", "throw \"A\";"),
    ("function", "sub f() {\n    throw \"B\";\n}\nf();"),
    ("nested statements", "sub g(int x) {\n    if (x) {\n        throw \"C\";\n    }\n}\ng(1);"),
    ("top-level builtin", "int z = 0;\nint y = 1 / z;"),
    ("builtin in a function", "sub h() {\n    int z = 0;\n    int y = 1 / z;\n}\nh();"),
    ("top-level try", "try {\n    throw \"E\";\n} catch (hash<ExceptionInfo> ex) {\n}\nthrow \"F\";"),
    ("caught from a function",
        "sub k() {\n    throw \"G\";\n}\ntry {\n    k();\n} catch (hash<ExceptionInfo> ex) {\n}"),
    ("top-level loop", "for (int i = 0; i < 1; ++i) {\n    throw \"H\";\n}"),
    ("method", "class C {\n    m() {\n        throw \"I\";\n    }\n}\nC c();\nc.m();"),
    ("rethrow in a catch block", "try {\n    throw \"J\";\n} catch (hash<ExceptionInfo> ex) {\n    throw \"K\";\n}"),
    ("loop in a function",
        "sub l() {\n    foreach int i in (1, 2) {\n        if (i == 2) {\n            throw \"L\";\n        }\n"
        "    }\n}\nl();"),
);

foreach list<string> p in (Programs) {
    ExceptionRecorder d();
    Program pgm(PO_NEW_STYLE | PO_ALLOW_DEBUGGER);
    pgm.parse("%modern\n" + p[1] + "\n", "t");
    d.addProgram(pgm.getProgram());
    try {
        pgm.run();
    } catch (hash<ExceptionInfo> ex) {
    }
    d.removeProgram(pgm.getProgram());
    printf("%s: %y\n", p[0], d.seen);
}
