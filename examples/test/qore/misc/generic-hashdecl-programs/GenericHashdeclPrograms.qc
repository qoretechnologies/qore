# Copyright (c) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
#
# The cases of the GenericHashdeclPrograms-<n>.qtest tests: each runs one part of them, so that each takes a short
# time, and together they run every case (see addPartCase()).
#
# A generic hashdecl instantiated at run time for the classes, hashdecls and enums of a series of Programs, each
# destroyed after its calls, gives each Program the declarations for its own types: a later type can be given the
# address of a destroyed Program's type, and the declaration made for that type is not used for it; threads making
# the same declaration at the same time all get the one published (generic-hashdecl-programs.qr, whose Programs run in
# every execution mode, in a qore process in every execution mode and in AOT executables).  Each run of the fixture
# creates 800 Programs (200 in each execution mode), which takes about 8 s in a debug build, and about 28 s with
# synchronous JIT compilation at threshold 1, as each Program's code is then compiled to native code in each mode;
# the runs are split by the execution mode of the qore process (qore#6359).

class GenericHashdeclProgramsBase inherits QUnit::Test {
    private {
        string qore = realpath(TestInterpreter::qore_interpreter());
        string libdir = realpath(ENV.QORE_LIBDIR ?? dirname(qore));
        string qcc = ENV.QCC ?? libdir + "/qcc";
        string source = get_script_dir() + "/generic-hashdecl-programs.qr";
        const Expected = "ast: ok\nir: ok\njit: ok\ntiered: ok\n";

        #! the environment of the runs with synchronous JIT compilation at threshold 1
        const SyncJit = "QORE_JIT_SYNC_COMPILE=1 QORE_IR_THRESHOLD=1 QORE_JIT_THRESHOLD=1 ";

        #! the part of the cases that this test runs (see addPartCase())
        int part;
    }

    constructor(string name, int part) : Test(name, "1.0", \ARGV) {
        self.part = part;
        # the runs of the fixture by a qore process in each execution mode, without and with synchronous JIT
        # compilation; part 1 also has the AOT executables
        addPartCase(1, "execution mode ast", \executionMode(), ("ast", ""));
        addPartCase(1, "execution mode ast, synchronous JIT", \executionMode(), ("ast", SyncJit));
        addPartCase(1, "AOT", \aot());
        addPartCase(2, "execution mode ir", \executionMode(), ("ir", ""));
        addPartCase(2, "execution mode ir, synchronous JIT", \executionMode(), ("ir", SyncJit));
        addPartCase(3, "execution mode jit", \executionMode(), ("jit", ""));
        addPartCase(3, "execution mode jit, synchronous JIT", \executionMode(), ("jit", SyncJit));
        addPartCase(4, "execution mode tiered", \executionMode(), ("tiered", ""));
        addPartCase(4, "execution mode tiered, synchronous JIT", \executionMode(), ("tiered", SyncJit));
        set_return_value(main());
    }

    #! adds a case if it is in the part that this test runs
    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    private static string quote(string value) {
        return "'" + replace(value, "'", "'\"'\"'") + "'";
    }

    private string run(list<string> args, string prefix = "") {
        int status;
        string output = backquote(prefix + "LD_LIBRARY_PATH=" + quote(libdir)
            + " DYLD_LIBRARY_PATH=" + quote(libdir) + " "
            + (map quote($1), args).join(" ") + " 2>&1", \status);
        assertEq(0, status, output);
        return output;
    }

    executionMode(string mode, string control) {
        assertEq(Expected, run((qore, "--exec-mode=" + mode, source), control), control + mode);
    }

    aot() {
        TmpDir dir("qore-generic-hashdecl-programs-");
        foreach string level in (("-O0", "-O3")) {
            string executable = dir.path + "/fixture";
            run((qcc, level, "-o", executable, source));
            assertEq(Expected, run((executable,)), level);
        }
    }
}
