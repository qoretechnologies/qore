# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The cases of the HandlerReturnPastCap-<n>.qtest tests: each runs one part of them, so that each takes a short time,
# and together they run every case (see addPartCase()); the jit and tiered runs compile every function of the script
# to native code at its first call, as qcc compiles them, which takes most of the time: part 1 runs the script with the
# AST interpreter, the IR interpreter, on the IR tier and with JIT, part 2 with tiered execution and compiled with qcc

#! A return in a handler run out of line past the inline cap of the lowering replaces the value of the return (#6137)
/** As in the AST interpreter, where the handlers of a block run with the return value of the function, the value of a
    return statement in a handler replaces the value of the return whose block cleanups run the handler, and the
    replaced value is released when it is replaced.  The lowering lowers the handlers of an exit inline up to a cap
    (handler bodies nested 16 deep, 64 handler bodies per exit, #5992); past it, the handlers run out of line, and
    their returns replaced nothing: nested handlers 17 and 32 deep gave the value of the 16th, and 7 handlers of one
    scope whose conditions overlap gave the value of another handler than the AST interpreter.  The cases: handlers
    nested 15, 16, 17 and 32 deep whose outermost two and innermost two return a value, an object (the release of the
    replaced values), no value, or nothing (the return's own value survives); handlers in blocks of their own; a
    handler registered before the nested ones that replaces their value, also from a try statement; a foreach statement
    left by the return, which releases its element, the value handed over, after the handlers; and handlers of one
    scope with overlapping conditions, also on_exit, on_success and on_error handlers.  HandlerReturnPastCap.q
    gives the output of the AST interpreter in every execution mode, on the IR tier and compiled with qcc.  The control
    flow of a return past the cap (the end of the code around the handler's run) is #6552.
*/
class HandlerReturnPastCapBase inherits QUnit::Test {
    public {
        #! the output of HandlerReturnPastCap.q with the AST interpreter
        const Expected = "n15(-1)[0]: \"n15\" []
n15(-1)[1]: \"n15\" []
n15(0)[0]: \"body\" []
n15(0)[1]: \"body\" []
n15(1)[0]: \"n1\" []
n15(1)[1]: \"n1\" []
n15(2)[0]: \"n2\" []
n15(2)[1]: \"n2\" []
n15(14)[0]: \"n14\" []
n15(14)[1]: \"n14\" []
n15(15)[0]: \"n15\" []
n15(15)[1]: \"n15\" []
n16(-1)[0]: \"n16\" []
n16(-1)[1]: \"n16\" []
n16(0)[0]: \"body\" []
n16(0)[1]: \"body\" []
n16(1)[0]: \"n1\" []
n16(1)[1]: \"n1\" []
n16(2)[0]: \"n2\" []
n16(2)[1]: \"n2\" []
n16(14)[0]: \"body\" []
n16(14)[1]: \"body\" []
n16(15)[0]: \"n15\" []
n16(15)[1]: \"n15\" []
n16(16)[0]: \"n16\" []
n16(16)[1]: \"n16\" []
n17(-1)[0]: \"n17\" []
n17(-1)[1]: \"n17\" []
n17(0)[0]: \"body\" []
n17(0)[1]: \"body\" []
n17(1)[0]: \"n1\" []
n17(1)[1]: \"n1\" []
n17(2)[0]: \"n2\" []
n17(2)[1]: \"n2\" []
n17(14)[0]: \"body\" []
n17(14)[1]: \"body\" []
n17(16)[0]: \"n16\" []
n17(16)[1]: \"n16\" []
n17(17)[0]: \"n17\" []
n17(17)[1]: \"n17\" []
o17(-1)[0]: T:o17-17 [\"~o17-body\", \"~o17-1\", \"~o17-2\", \"~o17-16\", \"~o17-17\"]
o17(-1)[1]: T:o17-17 [\"~o17-body\", \"~o17-1\", \"~o17-2\", \"~o17-16\", \"~o17-17\"]
o17(0)[0]: T:o17-body [\"~o17-body\"]
o17(0)[1]: T:o17-body [\"~o17-body\"]
o17(1)[0]: T:o17-1 [\"~o17-body\", \"~o17-1\"]
o17(1)[1]: T:o17-1 [\"~o17-body\", \"~o17-1\"]
o17(2)[0]: T:o17-2 [\"~o17-body\", \"~o17-2\"]
o17(2)[1]: T:o17-2 [\"~o17-body\", \"~o17-2\"]
o17(14)[0]: T:o17-body [\"~o17-body\"]
o17(14)[1]: T:o17-body [\"~o17-body\"]
o17(16)[0]: T:o17-16 [\"~o17-body\", \"~o17-16\"]
o17(16)[1]: T:o17-16 [\"~o17-body\", \"~o17-16\"]
o17(17)[0]: T:o17-17 [\"~o17-body\", \"~o17-17\"]
o17(17)[1]: T:o17-17 [\"~o17-body\", \"~o17-17\"]
r17(-1)[0]: null []
r17(-1)[1]: null []
r17(0)[0]: null []
r17(0)[1]: null []
r17(1)[0]: null []
r17(1)[1]: null []
r17(2)[0]: \"r2\" []
r17(2)[1]: \"r2\" []
r17(14)[0]: null []
r17(14)[1]: null []
r17(16)[0]: \"r16\" []
r17(16)[1]: \"r16\" []
r17(17)[0]: null []
r17(17)[1]: null []
v17(0)[0]: \"v-body\" []
v17(0)[1]: \"v-body\" []
v17(1)[0]: null []
v17(1)[1]: null []
v17(16)[0]: null []
v17(16)[1]: null []
v17(17)[0]: null []
v17(17)[1]: null []
n32(-1)[0]: \"n32\" []
n32(-1)[1]: \"n32\" []
n32(0)[0]: \"body\" []
n32(0)[1]: \"body\" []
n32(1)[0]: \"n1\" []
n32(1)[1]: \"n1\" []
n32(2)[0]: \"n2\" []
n32(2)[1]: \"n2\" []
n32(14)[0]: \"body\" []
n32(14)[1]: \"body\" []
n32(31)[0]: \"n31\" []
n32(31)[1]: \"n31\" []
n32(32)[0]: \"n32\" []
n32(32)[1]: \"n32\" []
o32(-1)[0]: T:o32-32 [\"~o32-body\", \"~o32-1\", \"~o32-2\", \"~o32-31\", \"~o32-32\"]
o32(-1)[1]: T:o32-32 [\"~o32-body\", \"~o32-1\", \"~o32-2\", \"~o32-31\", \"~o32-32\"]
o32(0)[0]: T:o32-body [\"~o32-body\"]
o32(0)[1]: T:o32-body [\"~o32-body\"]
o32(1)[0]: T:o32-1 [\"~o32-body\", \"~o32-1\"]
o32(1)[1]: T:o32-1 [\"~o32-body\", \"~o32-1\"]
o32(2)[0]: T:o32-2 [\"~o32-body\", \"~o32-2\"]
o32(2)[1]: T:o32-2 [\"~o32-body\", \"~o32-2\"]
o32(14)[0]: T:o32-body [\"~o32-body\"]
o32(14)[1]: T:o32-body [\"~o32-body\"]
o32(31)[0]: T:o32-31 [\"~o32-body\", \"~o32-31\"]
o32(31)[1]: T:o32-31 [\"~o32-body\", \"~o32-31\"]
o32(32)[0]: T:o32-32 [\"~o32-body\", \"~o32-32\"]
o32(32)[1]: T:o32-32 [\"~o32-body\", \"~o32-32\"]
b17(-1)[0]: \"b17\" []
b17(-1)[1]: \"b17\" []
b17(0)[0]: \"b-body\" []
b17(0)[1]: \"b-body\" []
b17(15)[0]: \"b15\" []
b17(15)[1]: \"b15\" []
b17(16)[0]: \"b16\" []
b17(16)[1]: \"b16\" []
b17(17)[0]: \"b17\" []
b17(17)[1]: \"b17\" []
p(-2)[0]: \"outer\" [\"~p-body\", \"~p1\", \"~p16\", \"~p17\"]
p(-2)[1]: \"outer\" [\"~p-body\", \"~p1\", \"~p16\", \"~p17\"]
p(-1)[0]: T:p17 [\"~p-body\", \"~p1\", \"~p16\", \"~p17\"]
p(-1)[1]: T:p17 [\"~p-body\", \"~p1\", \"~p16\", \"~p17\"]
p(0)[0]: T:p-body [\"~p-body\"]
p(0)[1]: T:p-body [\"~p-body\"]
p(1)[0]: T:p1 [\"~p-body\", \"~p1\"]
p(1)[1]: T:p1 [\"~p-body\", \"~p1\"]
p(16)[0]: T:p16 [\"~p-body\", \"~p16\"]
p(16)[1]: T:p16 [\"~p-body\", \"~p16\"]
p(17)[0]: T:p17 [\"~p-body\", \"~p17\"]
p(17)[1]: T:p17 [\"~p-body\", \"~p17\"]
q(-2)[0]: \"outer-try\" [\"~q-body\", \"~q1\", \"~q16\", \"~q17\"]
q(-2)[1]: \"outer-try\" [\"~q-body\", \"~q1\", \"~q16\", \"~q17\"]
q(-1)[0]: T:q17 [\"~q-body\", \"~q1\", \"~q16\", \"~q17\"]
q(-1)[1]: T:q17 [\"~q-body\", \"~q1\", \"~q16\", \"~q17\"]
q(0)[0]: T:q-body [\"~q-body\"]
q(0)[1]: T:q-body [\"~q-body\"]
q(1)[0]: T:q1 [\"~q-body\", \"~q1\"]
q(1)[1]: T:q1 [\"~q-body\", \"~q1\"]
q(16)[0]: T:q16 [\"~q-body\", \"~q16\"]
q(16)[1]: T:q16 [\"~q-body\", \"~q16\"]
q(17)[0]: T:q17 [\"~q-body\", \"~q17\"]
q(17)[1]: T:q17 [\"~q-body\", \"~q17\"]
fe(0)[0]: T:fe-body [\"~fe-g\", \"~fe-body\"]
fe(0)[1]: T:fe-body [\"~fe-g\", \"~fe-body\"]
fe(1)[0]: T:fe-g [\"~fe-body\", \"~fe-g\"]
fe(1)[1]: T:fe-g [\"~fe-body\", \"~fe-g\"]
fe(17)[0]: T:fe-g [\"~fe-body\", \"~fe-g\"]
fe(17)[1]: T:fe-g [\"~fe-body\", \"~fe-g\"]
c7(0)[0]: \"c-body\" []
c7(0)[1]: \"c-body\" []
c7(1)[0]: \"c3\" []
c7(1)[1]: \"c3\" []
c7(2)[0]: \"c1\" []
c7(2)[1]: \"c1\" []
c7(3)[0]: \"c1\" []
c7(3)[1]: \"c1\" []
c7(4)[0]: \"c2\" []
c7(4)[1]: \"c2\" []
c7(5)[0]: \"c2\" []
c7(5)[1]: \"c2\" []
c7(6)[0]: \"c1\" []
c7(6)[1]: \"c1\" []
c7(7)[0]: \"c1\" []
c7(7)[1]: \"c1\" []
c7(8)[0]: \"c-body\" []
c7(8)[1]: \"c-body\" []
c7(15)[0]: \"c1\" []
c7(15)[1]: \"c1\" []
s(0)[0]: T:s-body [\"~s-body\"]
s(0)[1]: T:s-body [\"~s-body\"]
s(1)[0]: T:s4 [\"~s-body\", \"~s4\"]
s(1)[1]: T:s4 [\"~s-body\", \"~s4\"]
s(2)[0]: T:s1 [\"~s-body\", \"~s1\"]
s(2)[1]: T:s1 [\"~s-body\", \"~s1\"]
s(3)[0]: T:s1 [\"~s-body\", \"~s4\", \"~s1\"]
s(3)[1]: T:s1 [\"~s-body\", \"~s4\", \"~s1\"]
s(5)[0]: T:s4 [\"~s-body\", \"~s6\", \"~s4\"]
s(5)[1]: T:s4 [\"~s-body\", \"~s6\", \"~s4\"]
s(6)[0]: T:s1 [\"~s-body\", \"~s6\", \"~s1\"]
s(6)[1]: T:s1 [\"~s-body\", \"~s6\", \"~s1\"]
s(7)[0]: T:s1 [\"~s-body\", \"~s6\", \"~s4\", \"~s1\"]
s(7)[1]: T:s1 [\"~s-body\", \"~s6\", \"~s4\", \"~s1\"]
s(15)[0]: T:s1 [\"~s-body\", \"~s7\", \"~s6\", \"~s4\", \"~s3\", \"~s1\"]
s(15)[1]: T:s1 [\"~s-body\", \"~s7\", \"~s6\", \"~s4\", \"~s3\", \"~s1\"]
s(16)[0]: ex:S []
s(16)[1]: ex:S []
s(17)[0]: ex:S [\"~s8\"]
s(17)[1]: ex:S [\"~s8\"]
s(18)[0]: ex:S [\"~s5\"]
s(18)[1]: ex:S [\"~s5\"]
s(31)[0]: ex:S [\"~s8\", \"~s6\", \"~s5\", \"~s3\", \"~s2\"]
s(31)[1]: ex:S [\"~s8\", \"~s6\", \"~s5\", \"~s3\", \"~s2\"]
";

        #! the optimization levels of the AOT compiles
        const OptLevels = (0, 3);
    }

    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;

        string qore_bin;
        string libqore_dir;
        string qcc_bin;
        string script;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0") {
        self.part = part;
        qore_bin = TestInterpreter::qore_interpreter();
        libqore_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);
        qcc_bin = ENV.QCC ?? ENV.QORE_QCC ?? (is_file(libqore_dir + "/qcc") ? libqore_dir + "/qcc" : "qcc");
        script = get_script_dir() + "/HandlerReturnPastCap.q";
        foreach string mode in (ExecModeHarness::ExecModes) {
            addPartCase(mode == "tiered" ? 2 : 1, mode + ": a handler's return past the inline cap", \modeTest(),
                (mode,));
        }
        addPartCase(1, ExecModeHarness::IRTierName + ": a handler's return past the inline cap", \irTierTest());
        foreach int opt in (OptLevels) {
            addPartCase(2, sprintf("qcc -O%d: a handler's return past the inline cap", opt), \aotTest(), (opt,));
        }
        set_return_value(main());
    }

    #! adds the test case if it is in the part that this test runs
    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    modeTest(string mode) {
        string output = ExecModeHarness::run(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s --exec-mode=%s %s",
            libqore_dir, libqore_dir, qore_bin, mode, script));
        checkOutput(mode, output);
    }

    irTierTest() {
        string output = ExecModeHarness::run_ir_tier(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s "
            "--exec-mode=tiered %s", libqore_dir, libqore_dir, qore_bin, script));
        checkOutput(ExecModeHarness::IRTierName, output);
    }

    aotTest(int opt) {
        TmpDir tmp("qore-handler-return-past-cap-");
        string exe = tmp.path + DirSep + "handler-return-past-cap";
        string out = backquote(sprintf("QORE_LIBDIR=%s %s -O%d -o %s %s 2>&1", libqore_dir, qcc_bin, opt, exe,
            script));
        assertTrue(is_file(exe), "compiled the executable: " + out);
        string output = ExecModeHarness::run_any(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s",
            libqore_dir, libqore_dir, exe));
        checkOutput(sprintf("qcc -O%d", opt), output);
    }

    #! Checks the output of a run against the AST interpreter's, line by line
    private checkOutput(string mode, string output) {
        hash<string, string> expected = parseOutput(Expected);
        hash<string, string> actual = parseOutput(output);
        assertEq(keys expected, keys actual, mode + ": the cases of the output");
        foreach hash<auto> i in (expected.pairIterator()) {
            assertEq(i.value, actual{i.key}, sprintf("%s: %s as with the AST interpreter", mode, i.key));
        }
    }

    #! Returns the lines of an output by their \c "<case>(<arg>)[<call>]" label
    private static hash<string, string> parseOutput(string output) {
        hash<string, string> rv();
        foreach string line in (output.split("\n")) {
            if (!line.val()) {
                continue;
            }
            int i = line.find(": ");
            rv{i > 0 ? line.substr(0, i) : line} = i > 0 ? line.substr(i + 2) : "";
        }
        return rv;
    }
}
