# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The test class of the short string probes, included by the tests that use it (%include): the test requires QUnit,
# FsUtil, Util and examples/test/TestInterpreter.qm.

#! runs a probe script that passes inline short strings to native code (issues #7038, #7062)
/** A QoreValue holds a string of up to 6 bytes inline, with no QoreStringNode behind it; native code that reads
    a string value as a QoreStringNode after checking only its type dereferences a null pointer for such a value
    (and a debug build aborts on an assertion).  The probe script prints one line for each probe; it is run in
    each execution mode in a child process and as qcc-compiled executables (-O0, -O3), which hold the strings of a
    constant hash inline.  It runs in a temporary directory, where it can make files, and the modules in its own
    directory are found first.  In a debug build, it can make its strings inline in every mode with
    dbg_make_short_string(); a function \c ss() of the probe script returns that string in a debug build and the
    value of a constant hash otherwise.
*/
class ShortStringProbeTest inherits QUnit::Test {
    public {
        const ExecModes = ("ast", "ir", "jit", "tiered");
    }

    private {
        string probe;
        string expected;
        string qore_bin;
        string lib_dir;
        *string qcc_bin;
    }

    #! creates the test
    /** @param name the name of the test
        @param probe the path of the probe script
        @param expected the standard output of the probe script
    */
    constructor(string name, string probe, string expected) : QUnit::Test(name, "1.0", \ARGV) {
        self.probe = normalize_dir(probe);
        self.expected = expected;
        qore_bin = TestInterpreter::qore_interpreter();
        lib_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);
        qcc_bin = TestInterpreter::qcc_compiler();
        addTestCase("native code reads inline strings in every execution mode", \testModes());
        addTestCase("native code reads inline strings in qcc code", \testCompiled());
    }

    testModes() {
        TmpDir tmp("qore-short-string-probes-");
        string env = sprintf("QORE_JIT_SYNC_COMPILE=1 QORE_JIT_THRESHOLD=1 LD_LIBRARY_PATH=%s "
            "DYLD_LIBRARY_PATH=%s", quote(lib_dir), quote(lib_dir));
        foreach string mode in (ExecModes) {
            check(mode, sprintf("%s %s -b --exec-mode=%s %s", env, quote(qore_bin), mode, quote(probe)), tmp);
        }
        check("tiered, IR tier", sprintf("%s %s -b --exec-mode=tiered --jit-ir-threshold=1 "
            "--jit-jit-threshold=1000000 %s", env, quote(qore_bin), quote(probe)), tmp);
    }

    testCompiled() {
        if (!qcc_bin || !is_executable(qcc_bin)) {
            testSkip("qcc is not available");
        }
        TmpDir tmp("qore-short-string-probes-");
        string env = sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s", quote(lib_dir), quote(lib_dir));
        foreach string opt in (("-O0", "-O3")) {
            string exe = tmp.path + DirSep + "probes" + opt;
            int rc;
            string out = backquote(sprintf("QORE_LIBDIR=%s %s %s -o %s %s 2>&1", quote(lib_dir), quote(qcc_bin),
                opt, quote(exe), quote(probe)), \rc);
            assertEq(0, rc, sprintf("qcc %s compiled the executable: %s", opt, out));
            if (!is_file(exe)) {
                continue;
            }
            check("qcc " + opt, sprintf("%s %s -b", env, quote(exe)), tmp);
        }
    }

    #! runs the command in the temporary directory and checks its standard output
    /** Its standard error is shown if it fails; the modules in the directory of the probe script are found first
    */
    private check(string label, string cmd, TmpDir tmp) {
        string err_file = tmp.path + DirSep + "stderr";
        string module_dir = dirname(probe) + (ENV.QORE_MODULE_DIR ? ":" + ENV.QORE_MODULE_DIR : "");
        int rc;
        string out = backquote(sprintf("cd %s && QORE_MODULE_DIR=%s %s 2>%s", quote(tmp.path), quote(module_dir),
            cmd, quote(err_file)), \rc);
        string err = ReadOnlyFile::readTextFile(err_file) ?? "";
        assertEq(expected, out, sprintf("%s (exit code %d; stderr: %s)", label, rc, err));
        assertEq(0, rc, sprintf("%s exit code (stderr: %s)", label, err));
    }

    #! returns the string quoted for the shell
    private static string quote(string arg) {
        return "'" + replace(arg, "'", "'\\''") + "'";
    }
}
