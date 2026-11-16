# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the cases of the JitTeardownAtExit-<n>.qtest tests (qore#6390): each runs one part of them, so that each takes a
# short time, and together they run every case (see addPartCase())

#! issue #5842, #6296: qore_cleanup() waits for a background compile still in LLVM and then releases the JIT
/** A background compile that is still in LLVM when qore_cleanup() stops the compiler is detached; it skips what it can
    of LLVM, and QoreJIT::shutdown() at the end of qore_cleanup() waits for it before it releases the JIT (issue #6296).
    Releasing the JIT deregisters the EH frames that JITLink registered with libgcc for the compiled code: before, they
    stayed registered, libgcc freed its table of registered frames at exit, and the memory that it had allocated for
    each frame was lost ("definitely lost" in valgrind, from __register_frame), as was the memory of the JIT
    ("possibly lost").

    The script holds a background compile in LLVM at the end (a test hook of a debug libqore), and qore_cleanup() lets
    it go.  The test checks that the JIT is released in QoreJIT::shutdown() (QORE_JIT_TIMING output), and, when
    valgrind is available, that valgrind reports no leak and no error.

    The cases are in three parts (see addPartCase()), JitTeardownAtExit-<n>.qtest, as the runs under valgrind take
    almost all of the time (the whole test took 140 s alone in a debug build and was killed at the runner's 300 s
    limit on a busy machine, qore#6390): part 1 checks the release in JIT and tiered execution (a second each), part
    2 runs valgrind in JIT execution and part 3 in tiered execution, with a smaller workload of the script (the
    argument \c "min", see valgrindTest()).
*/
class JitTeardownAtExitBase inherits QUnit::Test {
    public {
        const Expected = "held: True result: 810";
        #! the output of the script with the argument "min", which the runs under valgrind use
        const ExpectedMin = "held: True result: 540";
        #! what QORE_JIT_TIMING reports when shutdown() releases the JIT after the held compile ended
        const Released = "[BG-JIT] released the JIT in shutdown() after the abandoned compile ended";
    }

    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;

        string qore_bin;
        string env;
        string script;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        qore_bin = TestInterpreter::qore_interpreter();
        string libqore_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);
        # background compiles, compiled at the first call
        env = sprintf("env -u QORE_JIT_SYNC_COMPILE -u DEBUGINFOD_URLS LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s "
            "QORE_LIBDIR=%s QORE_IR_THRESHOLD=1 QORE_JIT_THRESHOLD=1", libqore_dir, libqore_dir, libqore_dir);
        script = get_script_dir() + "/JitTeardownAtExit.qr";

        addPartCase(1, "released after the abandoned compile", \releasedTest());
        addPartCase(2, "valgrind, JIT execution", \valgrindTest(), "jit");
        addPartCase(3, "valgrind, tiered execution", \valgrindTest(), "tiered");
        set_return_value(main());
    }

    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! skips the test case without library debugging, which the script needs
    private checkDebugging() {
        if (!Option::HAVE_LIBRARY_DEBUGGING) {
            testSkip("dbg_hold_bg_compile() requires library debugging");
        }
    }

    #! the JIT is released in shutdown() after the held compile ended, in JIT and tiered execution
    private releasedTest() {
        checkDebugging();
        foreach string mode in (("jit", "tiered")) {
            int rc;
            string out = backquote(sprintf("%s QORE_JIT_TIMING=1 %s -b --exec-mode=%s %s 2>&1", env, qore_bin, mode,
                script), \rc);
            assertEq(0, rc, sprintf("%s: the process ends normally: %s", mode, out));
            assertTrue(out.find(Expected) >= 0, sprintf("%s: result: %s", mode, out));
            assertTrue(out.find(Released) >= 0, sprintf("%s: the JIT is released in shutdown(): %s", mode, out));
        }
    }

    #! valgrind reports no leak and no error in the given mode (issue #5842: memory of the EH frame registration was
    #! lost)
    /** The script runs with the argument \c "min" here: it compiles one function in the background (f1) before the
        compile of held() that is held in LLVM at the end, instead of two (f1, f2) in the runs of part 1, as each
        further compiled function adds about 3 to 4 s under valgrind in a debug build, to a run that takes 19 s
        (tiered) and 24 s (JIT, which also compiles the top-level code) for a script that calls no function
        (qore#6390).  One function is enough for the paths that valgrind checks: its compile completes and is
        promoted, so its code and the EH frames that JITLink registered for it are in the JIT when the process ends,
        and the abandoned compile of held() is let go and waited for in QoreJIT::shutdown(), which then releases the
        JIT and deregisters the frames; a second function only repeats the first.
    */
    private valgrindTest(string mode) {
        checkDebugging();
        if (is_file("/proc/self/maps") && ReadOnlyFile::readTextFile("/proc/self/maps") =~ /valgrind|vgpreload/) {
            testSkip("the test runs under valgrind already");
        }
        *string valgrind = find_valgrind();
        if (!valgrind) {
            testSkip("valgrind is not available");
        }
        int rc;
        string out = backquote(sprintf("%s QORE_PCRE2_NO_JIT=1 %s --leak-check=full "
            "--errors-for-leak-kinds=definite,possible --error-exitcode=99 %s -b --exec-mode=%s %s min 2>&1",
            env, valgrind, qore_bin, mode, script), \rc);
        assertEq(0, rc, sprintf("%s: valgrind reports no error: %s", mode, out));
        assertRegex("definitely lost: 0 bytes|no leaks are possible|All heap blocks were freed", out,
            mode + ": no memory is lost");
        assertTrue(out.find(ExpectedMin) >= 0, sprintf("%s: result: %s", mode, out));
    }

    #! returns the path of valgrind, if it is in the PATH
    private static *string find_valgrind() {
        foreach string dir in ((ENV.PATH ?? "").split(":")) {
            if (dir && is_executable(dir + "/valgrind")) {
                return dir + "/valgrind";
            }
        }
    }
}
