# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# the cases of the OptionalOperandArithmetic-<n>.qtest tests (qore#6226): each runs one part of them, so that each takes
# a short time, and together they run every case (see addPartCase())

#! arithmetic on operands of an or-nothing type (*int, *float) gives the result of the operator for NOTHING (qore#5889)
/** The AOT compiler specialized an operator whose operands were of an or-nothing type to native arithmetic, which
    converts NOTHING to 0 or 0.0: `*int + *int` of two values with no value gave 0 where the operator gives NOTHING,
    and `*float + int` of NOTHING and 3 gave 3.0 where the operator gives the other operand, the int 3.  Every mode,
    qcc executables included, must print the output of the AST.  The operators are applied to every combination of an
    int, a *int, a float and a *float, with and without a value, as binary operators and as compound assignments
    (\c binary.q and \c compound.q are generated; \c issue.q is the program of the issue).

    The programs are in \c optional-operand-arithmetic/.

    The cases are in five parts (see addPartCase()), OptionalOperandArithmetic-<n>.qtest, so that each runs for a short
    time (the JIT runs of \c binary.q and \c compound.q take 15 s and 11 s and the qcc compiles 1-5 s each in a debug
    build; the whole test took 50 s alone and 260 s when the machine was busy, qore#6226): part 1 checks the AST
    interpreter's output, the IR and tiered execution of every program, and the JIT run and the qcc executables of
    \c issue.q; parts 2 and 3 the JIT run and the qcc executables of \c binary.q, and parts 4 and 5 those of
    \c compound.q.
*/
class OptionalOperandArithmeticBase inherits QUnit::Test {
    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;

        string qore_bin = TestInterpreter::qore_interpreter();
        string dir = get_script_dir() + "/optional-operand-arithmetic";

        const Modes = ("ast", "ir", "jit", "tiered");
        const Programs = ("issue", "binary", "compound");
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        addPartCase(1, "the AST interpreter prints the expected output", \testAst());
        addPartCase(1, "the parts run every program in every mode", \testPartsCoverEverything());
        addPartCase(1, "IR and tiered execution of every program print the output of the AST", \testModes(),
            (Programs, ("ir", "tiered")));
        addPartCase(1, "JIT execution of issue prints the output of the AST", \testModes(), (("issue",), ("jit",)));
        addPartCase(1, "qcc executables of issue print the output of the AST", \testCompiled(),
            (("issue",), ("0", "3")));
        addPartCase(2, "JIT execution of binary prints the output of the AST", \testModes(),
            (("binary",), ("jit",)));
        addPartCase(3, "qcc executables of binary print the output of the AST", \testCompiled(),
            (("binary",), ("0", "3")));
        addPartCase(4, "JIT execution of compound prints the output of the AST", \testModes(),
            (("compound",), ("jit",)));
        addPartCase(5, "qcc executables of compound print the output of the AST", \testCompiled(),
            (("compound",), ("0", "3")));
        set_return_value(main());
    }

    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! checks that the programs and the modes that the parts run are the ones of the test (a new one needs a part)
    testPartsCoverEverything() {
        assertEq(("issue", "binary", "compound"), Programs, "the programs that the parts run");
        assertEq(("ast", "ir", "jit", "tiered"), Modes, "the modes that the parts run");
    }

    private string expected(string name) {
        return ReadOnlyFile::readTextFile(dir + "/" + name + ".expected");
    }

    private string run(string name, string mode) {
        hash<ExecModeHarness::ModeRunResult> r = ExecModeHarness::run_command(sprintf("%s -b --enable-debug "
            "--exec-mode=%s %s/%s.q", qore_bin, mode, dir, name));
        assertEq(0, r.rc, sprintf("%s %s: exit status (output: %y)", name, mode, r.out));
        return r.stdout;
    }

    testAst() {
        foreach string name in (Programs) {
            assertEq(expected(name), run(name, "ast"), name + " ast");
        }
    }

    testModes(list<string> programs, list<string> modes) {
        foreach string name in (programs) {
            foreach string mode in (modes) {
                assertEq(expected(name), run(name, mode), name + " " + mode);
            }
        }
    }

    testCompiled(list<string> programs, list<string> opts) {
        string qcc_bin = ENV.QCC_BIN ?? dirname(qore_bin) + "/qcc";
        if (!is_file(qcc_bin)) {
            qcc_bin = "qcc";
        }
        TmpDir tmp("qore-optional-operand-arithmetic-");
        foreach string name in (programs) {
            foreach string opt in (opts) {
                string exe = sprintf("%s/%s-O%s", tmp.path, name, opt);
                int rc;
                string out = backquote(sprintf("%s -O%s -o %s %s/%s.q 2>&1", qcc_bin, opt, exe, dir, name), \rc);
                if (!is_file(exe)) {
                    # only a missing qcc skips the test: a program that qcc cannot compile is a failure
                    if (!is_file(qcc_bin) && rc == 127) {
                        testSkip("qcc is not available");
                    }
                    assertTrue(False, sprintf("%s -O%s: qcc cannot compile the program (rc %d): %s", name, opt, rc, out));
                    continue;
                }
                # -b must be the first argument of a compiled program
                string result = backquote(sprintf("%s -b 2>&1", exe), \rc);
                assertEq(0, rc, sprintf("%s -O%s: exit status (output: %y)", name, opt, result));
                assertEq(expected(name), result, sprintf("%s -O%s", name, opt));
            }
        }
    }
}
