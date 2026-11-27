# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

/*  The cases of the PipelineLifecycle-<n>.qtest tests: each runs one part of them, so that each takes a short time,
    and together they run every case (see addPartCase()).

    A program waits for all of its threads before it releases its global variables; the queue threads of a
    DataProviderPipeline waited for data until the pipeline's destructor stopped them, so a script with a pipeline in
    a global variable never ended (qore#7771).  The queue threads now run only while the pipeline processes data.  A
    thread callback that raised an error deadlocked the pipeline: the thread reported the error with the pipeline's
    lock, which the thread that started it held while it waited for it (qore#7777); the error is now raised by the
    call that starts the run.  The end of a run, also by several threads at once and with submissions during it,
    must end every waiter, every queue thread, and process every record that it accepted.  Each case runs
    PipelineLifecycle/pipeline-lifecycle.qr in a child process in each execution mode, with a time limit, and checks
    its output and that it ends with exit status 0.
*/

class PipelineLifecycleBase inherits QUnit::Test {
    public {
        #! the time limit of a child in seconds; a child that hangs is ended with exit status 124
        const TimeLimit = 120;

        #! the iterations of a race case in each execution mode
        /** With the code that had the bugs, a waiter that never returned (race-abort, race-waitdone), a run that
            never ended (race-flush, a stress check now: flush-window forces its window), and an accepted
            record that was never processed (late-submit, late-submit-buf; within 60 iterations in most runs, and in
            the first ones with the buffering processor) happened within these iterations in most runs of each case;
            the native execution modes run the most iterations, as they run them fastest
        */
        const RaceIterations = {
            "race-abort": {"ast": 300, "ir": 300, "jit": 300, "tiered": 300},
            "race-waitdone": {"ast": 300, "ir": 300, "jit": 300, "tiered": 300},
            "race-flush": {"ast": 100, "ir": 100, "jit": 300, "tiered": 300},
            "late-submit": {"ast": 100, "ir": 100, "jit": 300, "tiered": 300},
            "late-submit-buf": {"ast": 100, "ir": 100, "jit": 300, "tiered": 300},
        };

        #! the output of each race case, with the number of iterations
        const RaceExpected = {
            "race-abort": "race-abort: all waiters returned in %d iterations\n",
            "race-waitdone": "race-waitdone: all waiters returned in %d iterations\n",
            "race-flush": "race-flush: every run ended in %d iterations\n",
            "late-submit": "late-submit: every accepted record was processed in %d iterations (all accepted)\n",
            "late-submit-buf": "late-submit-buf: every accepted record was processed in %d iterations "
                "(all accepted)\n",
        };

        #! the output of each case
        const Expected = {
            "global": "records: [2]\n",
            "top-level": "records: [2]\n",
            "global-idle": "processing: False\n",
            "global-fanout": "callbacks before the run: 0\n"
                "run 0: records: [12, 13, 22, 23] callbacks: 3\n"
                "run 1: records: [12, 13, 22, 23] callbacks: 6\n",
            "global-abort": "processing: False\n",
            "global-copy": "records: [2]\n",
            "callback-throws": "constructed: callbacks: 0\n"
                "submit error: CB-ERROR: callback 1 failed\n"
                "processing: False\n"
                "errors logged: 1 (callback error: True)\n"
                "submit error: CB-ERROR: callback 2 failed\n"
                "processing: False\n"
                "errors logged: 1 (callback error: True)\n"
                "destroyed\n",
            "callback-throws-fanout": "submit error: CB-ERROR: callback 2 failed\n"
                "processing: False\n"
                "errors logged: 1 (callback error: True)\n"
                "records: [2, 12] callbacks: 5\n",
            "callback-throws-global": "submit error: CB-ERROR: callback 1 failed\n"
                "processing: False\n"
                "errors logged: 1 (callback error: True)\n",
            "flush-window": "run ended: processed 1 and 1\n",
            "sync-count": "submit(2): PIPELINE-SUBMISSION-ABORTED\n"
                "record_count: 1\n"
                "waitDone: PIPELINE-FAILED\n",
        };
    }

    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;

        string qore_bin;
        string libqore_dir;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        qore_bin = TestInterpreter::qore_interpreter();
        libqore_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);

        addPartCase(1, "pipelines in global variables end with the program", \testGlobal());
        addPartCase(1, "thread callback errors are raised", \testCallbackErrors());
        addPartCase(1, "records refused by a synchronous pipeline are not counted", \testSyncCount());
        addPartCase(2, "waiters return when another thread aborts the run", \testRace(), "race-abort");
        addPartCase(3, "waiters return when another thread ends the run", \testRace(), "race-waitdone");
        addPartCase(4, "runs of a fan-out pipeline that flushes end when several threads end them", \testRace(),
            "race-flush");
        addPartCase(4, "a run ends when it stops a queue whose thread has released the lock after its flush",
            \testFlushWindow());
        addPartCase(5, "records submitted while the run ends are processed", \testRace(), "late-submit");
        addPartCase(6, "records submitted while the run of a buffering pipeline ends are processed", \testRace(),
            "late-submit-buf");

        set_return_value(main());
    }

    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    testGlobal() {
        map runCase($1), ("global", "top-level", "global-idle", "global-fanout", "global-abort", "global-copy");
    }

    testCallbackErrors() {
        map runCase($1), ("callback-throws", "callback-throws-fanout", "callback-throws-global");
    }

    #! the end of the run stops queue 0 while its thread has released the lock after its flush (debug-only hooks)
    testFlushWindow() {
        runCase("flush-window");
    }

    testSyncCount() {
        runCase("sync-count");
    }

    #! a race case: threads that end the run at once (race-abort, race-waitdone, race-flush), or a submission while
    #! the run ends (late-submit, late-submit-buf)
    testRace(string c) {
        runCase(c, RaceIterations{c});
    }

    #! runs a case in each execution mode, with the iterations of the mode for a race case
    private runCase(string c, *hash<string, int> iterations) {
        string script = get_script_dir() + DirSep + "PipelineLifecycle" + DirSep + "pipeline-lifecycle.qr";
        foreach string mode in (ExecModeHarness::ExecModes) {
            *int n = iterations{mode};
            string cmd = sprintf("LD_LIBRARY_PATH=%s timeout %d %s -b --enable-debug --exec-mode=%s %s %s %s",
                libqore_dir, TimeLimit, qore_bin, mode, script, c, n ?? "");
            hash<ModeRunResult> r = ExecModeHarness::run_command(cmd);
            ExecModeHarness::check_loader_warnings(r);
            string label = sprintf("%s (%s)", c, mode);
            assertEq(0, r.rc, label + ": exit status (124: the program did not end): " + r.out);
            # a case that needs the debug-only test hooks of the module skips without them
            if (*string reason = (r.out =~ x/^skipped: ([^\n]*)/)[0]) {
                testSkip("%s (%s): the module has no test hooks: %s", c, mode, reason);
            }
            string expected = exists n ? sprintf(RaceExpected{c}, n) : Expected{c};
            assertEq(expected + "end of script\n", r.out, label);
            ExecModeHarness::check_native(r, NativeCheck::Jit);
        }
    }
}
