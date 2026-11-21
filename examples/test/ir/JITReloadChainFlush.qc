# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# the cases of the JITReloadChainFlush-<n>.qtest tests (qore#6409): each runs one part of them, so that each takes a
# short time, and together they run every case (see addPartCase())

#! The flush of the reload chains of a function with many tracked locals (qore#6299)
/** At each temp boundary, the native code of a function rotates the reload tracker of each tracked local into its
    deferred slot and releases the old deferred value.  A function over QORE_JIT_RELOAD_CHAIN_CALL_THRESHOLD tracked
    locals made a call for each local at each boundary, and a store for each local's valid epoch: its code grew with the
    product of its tracked locals and its boundaries, and its code generation took time in more than that.  The flush is
    one call with a table now.  This test generates functions with many catch blocks that have a local, an object local
    and a handler, checks that the code of the function grows linearly with the catch blocks, and that the results and
    the order of the destructors are those of the AST in every mode, and in executables made by qcc at -O0 and -O3.

    The tables of the helpers give their slots by their indexes in the cleanup frame of the function: the entry block
    stored the address of each slot in arrays, thousands of stores without a call between them that made LLVM's code
    generation superlinear; the test checks that the entry block stores no address.

    The cases are in five parts (see addPartCase()), JITReloadChainFlush-<n>.qtest, so that each runs for a short
    time (the whole test took 112 s alone in a debug build and was killed at the runner's 300 s limit under load,
    qore#6409; part 2 with the inline flush and the qcc executables took 70 s of CPU time alone in a debug build, 27 s of
    them for the inline flush, and was killed at the limit under load): part 1 checks the growth of the code, part 2
    the results in every mode, part 3 a function under the threshold and the entry block, part 4 the results with the
    inline flush, and part 5 the results in executables made by qcc at -O0 and -O3.
*/
class JITReloadChainFlushBase inherits QUnit::Test {
    public {
        #! the catch blocks of the smaller and of the larger function
        const Small = 40;
        const Large = 80;

        #! the largest ratio of the calls of the larger and of the smaller function (2.0 when linear; 2.47 with a call
        #! for each tracked local at each boundary)
        const MaxCallRatio = 2.2;
    }

    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;

        string qore_bin;
        string qcc_bin;
        #! the library path of the qcc compiles and of the executables
        string lib_env;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        qore_bin = TestInterpreter::qore_interpreter();
        string libqore_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);
        qcc_bin = TestInterpreter::qcc_compiler() ?? "qcc";
        lib_env = sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s", libqore_dir, libqore_dir);
        addPartCase(1, "the code of a function with many tracked locals grows linearly", \testLinear());
        addPartCase(2, "the flushes keep the results and the order of the destructors of the AST", \testResults());
        addPartCase(3, "a function under the threshold keeps the inline flush", \testInline());
        addPartCase(3, "the entry block of a large function stores no slot address", \testEntryBlock());
        addPartCase(4, "the inline flush keeps the results and the order of the destructors of the AST",
            \testResultsInline());
        addPartCase(5, "the flushes of a qcc -O0 executable keep the results of the AST", \testResultsQcc(), "-O0");
        addPartCase(5, "the flushes of a qcc -O3 executable keep the results of the AST", \testResultsQcc(), "-O3");
        set_return_value(main());
    }

    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! Returns a script with a function of \a n catch blocks, each with an int and an object local and a handler
    private string getCode(int n) {
        string code = "%modern

string log;

class D {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { log += \"~\" + n; }
    string get() { return n; }
}

int sub may_throw(int i) { if (i < 0) { throw \"NEG\", \"negative\"; } return i + 1; }

string sub big(int n) {
    int r = 0;
    *D keep;
";
        for (int i = 0; i < n; ++i) {
            code += sprintf("    try {
        r += may_throw(n - %d);
        keep = new D(\"t%d\");
    } catch (hash<ExceptionInfo> ex) {
        int l%d = r + %d;
        D d%d = new D(\"c%d\");
        on_exit { r += l%d; log += \"x%d\"; }
        r += d%d.get().size();
    }
", i % 7 == 3 ? 1000 : 0, i, i, i, i, i, i, i, i);
        }
        code += "    return sprintf(\"%d:%s\", r, keep.get());
}

for (int c = 0; c < 3; ++c) {
    log = \"\";
    string v = big(c);
    printf(\"rv%d:%s %s\\n\", c, v, log);
}
";
        return code;
    }

    #! Writes \a code to a temporary file and returns its output in \a exec_mode with the environment \a env
    private string runScript(string code, string exec_mode, string env) {
        TmpFile tmp(NOTHING, ".qr");
        tmp.file.write(code);
        tmp.file.close();
        return ExecModeHarness::run(sprintf("%s QORE_JIT_SYNC_COMPILE=1 QORE_IR_THRESHOLD=1 QORE_JIT_THRESHOLD=1 "
            "QORE_JIT_MAX_IR_INSTRUCTIONS=0 QORE_JIT_MAX_HANDLER_COST=0 %s --enable-debug --exec-mode=%s %s", env,
            qore_bin, exec_mode, tmp.path));
    }

    #! Returns the result lines of the output
    private string getResults(string out) {
        string rv;
        foreach string line in (out.split("\n")) {
            if (line =~ /^rv[0-9]:/) {
                rv += line + "\n";
            }
        }
        return rv;
    }

    #! Returns the number of each call of the native code of function \c big in the LLVM dump in \a out
    /** JIT: the function \c big@<id>_0; AOT: \c big_<signature>__<hash> and the functions outlined from it, without
        the functions of its handlers (\c _obe_<n>); a dump of the module before and after its optimization counts
        twice
    */
    private hash<string, int> getCalls(string out, string what) {
        hash<string, int> calls;
        bool in_big;
        foreach string line in (out.split("\n")) {
            if (line =~ /^define .* @"?big(@[0-9]+_0|_[A-Za-z0-9_]+)"?\(/ && line !~ /_obe_[0-9]/) {
                in_big = True;
                continue;
            }
            if (!in_big) {
                continue;
            }
            if (line =~ /^}/) {
                in_big = False;
                continue;
            }
            *list<string> m = (line =~ x/ (?:call|invoke) [^@]*@"?([A-Za-z_0-9.]+)/);
            if (m) {
                ++calls{m[0]};
                ++calls."*";
            }
        }
        if (!calls) {
            throw "NO-DUMP", sprintf("no LLVM function 'big' in the output for %s (%d bytes)", what, out.size());
        }
        return calls;
    }

    #! Returns the entry blocks of the native code of function \c big in the LLVM dump in \a out (see getCalls())
    private list<string> getEntryBlocks(string out, string what) {
        list<string> rv;
        *string entry;
        int labels;
        foreach string line in (out.split("\n")) {
            if (line =~ /^define .* @"?big(@[0-9]+_0|_[A-Za-z0-9_]+)"?\(/ && line !~ /_obe_[0-9]/
                    && line !~ /_fast_?/) {
                entry = "";
                labels = 0;
                continue;
            }
            if (!exists entry) {
                continue;
            }
            if (line =~ /^}/ || (line =~ /^[^ ;].*:/ && ++labels > 1)) {
                rv += entry;
                remove entry;
                continue;
            }
            entry += line + "\n";
        }
        if (!rv) {
            throw "NO-DUMP", sprintf("no LLVM function 'big' in the output for %s (%d bytes)", what, out.size());
        }
        return rv;
    }

    #! Checks that the entry blocks have a cleanup frame and store no address
    private checkEntryBlocks(list<string> entries, string what) {
        foreach string entry in (entries) {
            # (an array type once optimized)
            assertRegex("%cleanup_frame[0-9]* = alloca (i64, i64 [0-9]+|\\[[0-9]+ x i64\\])", entry,
                what + ": the function has a frame");
            assertEq(0, (entry =~ x/(store ptr %)/g ?? ()).size(), what + ": no address is stored in the entry block");
        }
    }

    testEntryBlock() {
        foreach int n in ((Small, Large)) {
            string what = sprintf("%d catch blocks", n);
            checkEntryBlocks(getEntryBlocks(runScript(getCode(n), "jit", "QORE_DUMP_LLVM_IR=1"), what), what);
        }
        # in executables made by qcc as well
        TmpDir dir();
        string src = join_paths(dir.path, "entry.qr");
        File f();
        f.open2(src, O_CREAT | O_WRONLY | O_TRUNC);
        f.write(getCode(Small));
        f.close();
        foreach string opt in (("-O0", "-O3")) {
            string exe = join_paths(dir.path, "entry" + opt);
            string out = backquote(sprintf("%s QORE_DUMP_LLVM_IR=1 %s %s -o %s %s 2>&1", lib_env, qcc_bin, opt,
                exe, src));
            assertTrue(is_executable(exe), sprintf("qcc %s made an executable: %s", opt, out.substr(-2000)));
            checkEntryBlocks(getEntryBlocks(out, "qcc " + opt), "qcc " + opt);
        }
    }

    testLinear() {
        hash<string, int> small = getCalls(runScript(getCode(Small), "jit", "QORE_DUMP_LLVM_IR=1"), "small");
        hash<string, int> large = getCalls(runScript(getCode(Large), "jit", "QORE_DUMP_LLVM_IR=1"), "large");
        foreach hash<string, int> calls in ((small, large)) {
            # no call for each local at a boundary
            assertEq(0, calls.qore_rt_reload_chain_rotate ?? 0, "no rotation of a single local");
            assertEq(0, calls.qore_rt_reload_chain_clear ?? 0, "no clear of a single local");
        }
        assertGt(0, large.qore_rt_reload_chain_flush ?? 0, "the flushes are table calls");
        float ratio = float(large."*") / small."*";
        assertTrue(ratio <= MaxCallRatio, sprintf("the calls of the function grow by %0.2f when its catch blocks "
            "double (at most %0.2f): %d -> %d", ratio, MaxCallRatio, small."*", large."*"));
    }

    testResults() {
        string code = getCode(Large);
        string expected = getExpectedResults(code);
        foreach string mode in (("ir", "jit", "tiered")) {
            assertEq(expected, getResults(runScript(code, mode, "")), mode);
        }
    }

    #! the inline form for every function gives the same results
    testResultsInline() {
        string code = getCode(Large);
        string expected = getExpectedResults(code);
        assertEq(expected, getResults(runScript(code, "jit", "QORE_JIT_RELOAD_CHAIN_CALL_THRESHOLD=0")),
            "jit with the inline flush");
    }

    #! checks an executable made by qcc with the given optimization level
    testResultsQcc(string opt) {
        string code = getCode(Large);
        string expected = getExpectedResults(code);
        TmpDir dir();
        string src = join_paths(dir.path, "flush.qr");
        File f();
        f.open2(src, O_CREAT | O_WRONLY | O_TRUNC);
        f.write(code);
        f.close();
        string exe = join_paths(dir.path, "flush" + opt);
        string out = backquote(sprintf("%s QORE_DUMP_LLVM_IR=1 %s %s -o %s %s 2>&1", lib_env, qcc_bin, opt,
            exe, src));
        assertTrue(is_executable(exe), sprintf("qcc %s made an executable: %s", opt, out.substr(-2000)));
        # the AOT code takes the table path too
        hash<string, int> calls = getCalls(out, "qcc " + opt);
        assertGt(0, calls.qore_rt_reload_chain_flush ?? 0, "qcc " + opt + ": the flushes are table calls");
        assertEq(0, calls.qore_rt_reload_chain_rotate ?? 0, "qcc " + opt + ": no rotation of a single local");
        assertEq(expected, getResults(backquote(sprintf("%s %s 2>&1", lib_env, exe))), "qcc " + opt);
    }

    #! Returns the results of the code with the AST interpreter, which the other modes must give
    private string getExpectedResults(string code) {
        string expected = getResults(runScript(code, "ast", ""));
        assertRegex("^rv0:", expected);
        assertEq(3, expected.split("\n").size(), "three result lines");
        return expected;
    }

    testInline() {
        # a function with few tracked locals flushes them inline, without a call
        hash<string, int> calls = getCalls(runScript(getCode(4), "jit", "QORE_DUMP_LLVM_IR=1"), "four");
        assertEq(0, calls.qore_rt_reload_chain_flush ?? 0, "no table call under the threshold");
        string code = getCode(4);
        string expected = getResults(runScript(code, "ast", ""));
        assertEq(expected, getResults(runScript(code, "jit", "")), "jit");
        # the table call for every function gives the same
        assertEq(expected, getResults(runScript(code, "jit", "QORE_JIT_RELOAD_CHAIN_CALL_THRESHOLD=1")),
            "jit with the table call");
        calls = getCalls(runScript(code, "jit", "QORE_DUMP_LLVM_IR=1 QORE_JIT_RELOAD_CHAIN_CALL_THRESHOLD=1"),
            "four, table call");
        assertGt(0, calls.qore_rt_reload_chain_flush ?? 0, "the table call with a threshold of 1");
    }
}
