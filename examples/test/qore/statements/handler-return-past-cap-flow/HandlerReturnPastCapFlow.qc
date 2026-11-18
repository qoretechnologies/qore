# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The cases of the HandlerReturnPastCapFlow-<n>.qtest tests: each runs one part of them, so that each takes a short time,
# and together they run every case (see addPartCase()); the jit and tiered runs compile every function of the script
# to native code at its first call, as qcc compiles them, which takes most of the time: part 1 runs the script with the
# AST interpreter, the IR interpreter, on the IR tier and with JIT, part 2 with tiered execution and compiled with qcc

#! A return in a handler run out of line past the inline cap of the lowering ends the code around the run (#6552)
/** In the AST interpreter, a return statement in a handler ends the code around the handler's run as a return
    statement there would: the block that runs the handler gives the return code of a return, so that the statements
    after it are not run, and the function returns the handler's value.  The handlers that the lowering runs out of
    line past the inline cap of handlers (#5992) replace the value of the return whose cleanups run them (#6137), but
    at the end of a block, a break or a continue in the body of a handler they ended nothing: the code after the block
    ran, and with no return pending the function returned its own value.  The cases: handlers nested 16, 17 and 20 deep
    in a block of the function; statements after a block in each handler; a break and a continue in a loop of a handler
    past the cap; a block with a local and handlers past the cap (its handlers run, then its local is released, then the
    function returns); a handler past the cap in a loop of a handler; a function and a closure called from handlers
    past the cap that return from their own handlers, which does not end the caller's code; an on_error handler, run as
    a function of its own, with handlers nested 16 and 17 deep and a statement after their block; and two threads that
    run handlers past the cap at the same time.  HandlerReturnPastCapFlow.q gives the output of the AST interpreter in
    every execution mode, on the IR tier and compiled with qcc.
*/
class HandlerReturnPastCapFlowBase inherits QUnit::Test {
    public {
        #! the output of HandlerReturnPastCapFlow.q with the AST interpreter
        const Expected = "blk16(0)[0]: \"body\" [\"after-block\"]
blk16(0)[1]: \"body\" [\"after-block\"]
blk16(1)[0]: \"h1\" []
blk16(1)[1]: \"h1\" []
blk16(15)[0]: \"h15\" []
blk16(15)[1]: \"h15\" []
blk16(16)[0]: \"h16\" []
blk16(16)[1]: \"h16\" []
blk17(0)[0]: \"body\" [\"after-block\"]
blk17(0)[1]: \"body\" [\"after-block\"]
blk17(1)[0]: \"h1\" []
blk17(1)[1]: \"h1\" []
blk17(16)[0]: \"h16\" []
blk17(16)[1]: \"h16\" []
blk17(17)[0]: \"h17\" []
blk17(17)[1]: \"h17\" []
blk20(0)[0]: \"body\" [\"after-block\"]
blk20(0)[1]: \"body\" [\"after-block\"]
blk20(1)[0]: \"h1\" []
blk20(1)[1]: \"h1\" []
blk20(19)[0]: \"h19\" []
blk20(19)[1]: \"h19\" []
blk20(20)[0]: \"h20\" []
blk20(20)[1]: \"h20\" []
aft16(0)[0]: \"body\" [\"after16\", \"after15\", \"after14\", \"after13\", \"after12\", \"after11\", \"after10\", \"after9\", \"after8\", \"after7\", \"after6\", \"after5\", \"after4\", \"after3\", \"after2\", \"after1\"]
aft16(0)[1]: \"body\" [\"after16\", \"after15\", \"after14\", \"after13\", \"after12\", \"after11\", \"after10\", \"after9\", \"after8\", \"after7\", \"after6\", \"after5\", \"after4\", \"after3\", \"after2\", \"after1\"]
aft16(1)[0]: \"h1\" [\"after16\", \"after15\", \"after14\", \"after13\", \"after12\", \"after11\", \"after10\", \"after9\", \"after8\", \"after7\", \"after6\", \"after5\", \"after4\", \"after3\", \"after2\"]
aft16(1)[1]: \"h1\" [\"after16\", \"after15\", \"after14\", \"after13\", \"after12\", \"after11\", \"after10\", \"after9\", \"after8\", \"after7\", \"after6\", \"after5\", \"after4\", \"after3\", \"after2\"]
aft16(2)[0]: \"h2\" [\"after16\", \"after15\", \"after14\", \"after13\", \"after12\", \"after11\", \"after10\", \"after9\", \"after8\", \"after7\", \"after6\", \"after5\", \"after4\", \"after3\"]
aft16(2)[1]: \"h2\" [\"after16\", \"after15\", \"after14\", \"after13\", \"after12\", \"after11\", \"after10\", \"after9\", \"after8\", \"after7\", \"after6\", \"after5\", \"after4\", \"after3\"]
aft16(15)[0]: \"h15\" [\"after16\"]
aft16(15)[1]: \"h15\" [\"after16\"]
aft16(16)[0]: \"h16\" []
aft16(16)[1]: \"h16\" []
aft17(0)[0]: \"body\" [\"after17\", \"after16\", \"after15\", \"after14\", \"after13\", \"after12\", \"after11\", \"after10\", \"after9\", \"after8\", \"after7\", \"after6\", \"after5\", \"after4\", \"after3\", \"after2\", \"after1\"]
aft17(0)[1]: \"body\" [\"after17\", \"after16\", \"after15\", \"after14\", \"after13\", \"after12\", \"after11\", \"after10\", \"after9\", \"after8\", \"after7\", \"after6\", \"after5\", \"after4\", \"after3\", \"after2\", \"after1\"]
aft17(1)[0]: \"h1\" [\"after17\", \"after16\", \"after15\", \"after14\", \"after13\", \"after12\", \"after11\", \"after10\", \"after9\", \"after8\", \"after7\", \"after6\", \"after5\", \"after4\", \"after3\", \"after2\"]
aft17(1)[1]: \"h1\" [\"after17\", \"after16\", \"after15\", \"after14\", \"after13\", \"after12\", \"after11\", \"after10\", \"after9\", \"after8\", \"after7\", \"after6\", \"after5\", \"after4\", \"after3\", \"after2\"]
aft17(2)[0]: \"h2\" [\"after17\", \"after16\", \"after15\", \"after14\", \"after13\", \"after12\", \"after11\", \"after10\", \"after9\", \"after8\", \"after7\", \"after6\", \"after5\", \"after4\", \"after3\"]
aft17(2)[1]: \"h2\" [\"after17\", \"after16\", \"after15\", \"after14\", \"after13\", \"after12\", \"after11\", \"after10\", \"after9\", \"after8\", \"after7\", \"after6\", \"after5\", \"after4\", \"after3\"]
aft17(16)[0]: \"h16\" [\"after17\"]
aft17(16)[1]: \"h16\" [\"after17\"]
aft17(17)[0]: \"h17\" []
aft17(17)[1]: \"h17\" []
brk(0)[0]: \"body\" [\"loop\", \"after-loop\"]
brk(0)[1]: \"body\" [\"loop\", \"after-loop\"]
brk(1)[0]: \"brk\" [\"loop\"]
brk(1)[1]: \"brk\" [\"loop\"]
cnt(0)[0]: \"body\" [\"i0\", \"i1\", \"i2\", \"after-loop\"]
cnt(0)[1]: \"body\" [\"i0\", \"i1\", \"i2\", \"after-loop\"]
cnt(1)[0]: \"cnt1\" [\"i0\", \"i1\"]
cnt(1)[1]: \"cnt1\" [\"i0\", \"i1\"]
nst(0)[0]: \"body\" [\"inner-body\", \"after-block\"]
nst(0)[1]: \"body\" [\"inner-body\", \"after-block\"]
nst(2)[0]: \"body\" [\"h17\", \"after-block\"]
nst(2)[1]: \"body\" [\"h17\", \"after-block\"]
nst(17)[0]: \"h17\" [\"inner-body\"]
nst(17)[1]: \"h17\" [\"inner-body\"]
cls(0)[0]: \"body\" [\"c-handler\", \"after-block\"]
cls(0)[1]: \"body\" [\"c-handler\", \"after-block\"]
cls(17)[0]: \"h17\" [\"c-handler\"]
cls(17)[1]: \"h17\" [\"c-handler\"]
hf16(0)[0]: ex:ERR [\"after-block\"]
hf16(0)[1]: ex:ERR [\"after-block\"]
hf16(16)[0]: ex:ERR []
hf16(16)[1]: ex:ERR []
hf17(0)[0]: ex:ERR [\"after-block\"]
hf17(0)[1]: ex:ERR [\"after-block\"]
hf17(17)[0]: ex:ERR []
hf17(17)[1]: ex:ERR []
thr: {\"h17\"/[\"after-block\"]: 400}
bord(0)[0]: T:body [\"h17\", \"~loc\", \"after-block\", \"~body\"]
bord(0)[1]: T:body [\"h17\", \"~loc\", \"after-block\", \"~body\"]
bord(1)[0]: T:ret17 [\"h17\", \"~loc\", \"~ret17\"]
bord(1)[1]: T:ret17 [\"h17\", \"~loc\", \"~ret17\"]
lp(0)[0]: T:lp-body [\"after-loop\", \"~lp-body\"]
lp(0)[1]: T:lp-body [\"after-loop\", \"~lp-body\"]
lp(1)[0]: T:lp-0 [\"~lp-body\", \"~lp-0\"]
lp(1)[1]: T:lp-0 [\"~lp-body\", \"~lp-0\"]
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
        script = get_script_dir() + "/HandlerReturnPastCapFlow.q";
        foreach string mode in (ExecModeHarness::ExecModes) {
            addPartCase(mode == "tiered" ? 2 : 1, mode + ": a handler's return past the inline cap ends the code", \modeTest(),
                (mode,));
        }
        addPartCase(1, ExecModeHarness::IRTierName + ": a handler's return past the inline cap ends the code", \irTierTest());
        foreach int opt in (OptLevels) {
            addPartCase(2, sprintf("qcc -O%d: a handler's return past the inline cap ends the code", opt), \aotTest(), (opt,));
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
        TmpDir tmp("qore-handler-return-past-cap-flow-");
        string exe = tmp.path + DirSep + "handler-return-past-cap-flow";
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
