# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (c) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

# The cases of the GenericSpecializationPrograms-<n>.qtest tests: each runs one part of them, so that each takes a
# short time, and together they run every case (see addPartCase())


class GenericSpecializationProgramsBase inherits QUnit::Test {
    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;
        string qore = realpath(TestInterpreter::qore_interpreter());
        string libdir = realpath(ENV.QORE_LIBDIR ?? dirname(qore));
        string qcc = ENV.QCC ?? libdir + "/qcc";
        string source = get_script_dir() + "/GenericSpecializationPrograms.qr";
        const Expected = "ast: ok\nir: ok\njit: ok\ntiered: ok\n";
    }

    constructor(string name, int part) : Test(name, "1.0", \ARGV) {
        self.part = part;
        addPartCase(1, "execution modes", \executionModes());
        addPartCase(2, "AOT", \aot());
        set_return_value(main());
    }

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

    executionModes() {
        foreach string mode in (("ast", "ir", "jit", "tiered")) {
            foreach string control in (("", "QORE_JIT_SYNC_COMPILE=1 QORE_IR_THRESHOLD=1 QORE_JIT_THRESHOLD=1 ")) {
                assertEq(Expected, run((qore, "--exec-mode=" + mode, source), control), control + mode);
            }
        }
    }

    aot() {
        TmpDir dir("qore-generic-spec-programs-");
        foreach string level in (("-O0", "-O3")) {
            string executable = dir.path + "/fixture";
            run((qcc, level, "-o", executable, source));
            assertEq(Expected, run((executable,)), level);
        }
    }
}
