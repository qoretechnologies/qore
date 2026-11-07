# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the base class of the tests that run a script in every execution mode and as a qcc executable and require the
# AST interpreter's output, given as the expected output

#! Runs a script in every execution mode and as a qcc-compiled executable, requiring the expected output
/** The script is run with the ExecModeHarness environment: native compilation is synchronous and follows the first
    call of a function (thresholds of 1), so tiered execution runs a function on the IR tier in its first call and as
    native code from its second call on, and JIT execution runs it as native code; a script that calls each function
    three times therefore runs it on every tier.  The AST interpreter is checked with <tt>--exec-mode=ast</tt>.

    The expected output is the AST interpreter's; every mode is run before the result is checked, so that a failure
    shows every mode whose output differs, line by line.
*/
class ExecModeOutputTest inherits QUnit::Test {
    private {
        #! the script
        string script;
        #! the expected output
        string expected;
        #! the file name prefix of the script and executable
        string prefix;

        string qore_bin;
        string libqore_dir;
        string qcc_bin;
    }

    #! creates the test; \a desc describes what every mode and the executable must give
    constructor(string name, string script, string expected, string desc) : QUnit::Test(name, "1.0", \ARGV) {
        self.script = script;
        self.expected = expected;
        prefix = name.lwr();
        qore_bin = ENV.QORE_BIN ?? ENV.QORE_BINARY ?? QORE_ARGV[0] ?? "qore";
        libqore_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);
        qcc_bin = ENV.QCC ?? ENV.QORE_QCC ?? ENV.QORE_QCC_EXECUTABLE
            ?? (is_file(libqore_dir + "/qcc") ? libqore_dir + "/qcc" : dirname(qore_bin) + "/qcc");
        if (!is_file(qcc_bin)) {
            qcc_bin = "qcc";
        }

        addTestCase("every execution mode " + desc, \testModes());
        addTestCase("compiled executable " + desc, \testAot());

        set_return_value(main());
    }

    #! writes the script into the directory given and returns its path
    private string writeScript(TmpDir tmp) {
        string path = tmp.path + DirSep + prefix + ".qr";
        File f();
        f.open2(path, O_WRONLY | O_CREAT | O_TRUNC);
        f.write(script);
        f.close();
        return path;
    }

    testModes() {
        TmpDir tmp("qore-" + prefix + "-");
        string path = writeScript(tmp);
        hash<string, list<string>> wrong();
        foreach string mode in (ExecModeHarness::ExecModes) {
            string actual = ExecModeHarness::run(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s -b "
                "--enable-debug --exec-mode=%s %s", libqore_dir, libqore_dir, qore_bin, mode, path));
            list<string> diff = differences(expected, actual);
            if (diff) {
                wrong{mode} = diff;
            }
        }
        assertEq({}, wrong);
    }

    testAot() {
        TmpDir tmp("qore-" + prefix + "-");
        string path = writeScript(tmp);
        string exe = tmp.path + DirSep + prefix;
        string out = backquote(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s -o %s %s 2>&1", libqore_dir,
            libqore_dir, qcc_bin, exe, path));
        assertTrue(is_file(exe), "compiled the executable: " + out);
        assertEq((), differences(expected, ExecModeHarness::run_any(sprintf("LD_LIBRARY_PATH=%s "
            "DYLD_LIBRARY_PATH=%s %s", libqore_dir, libqore_dir, exe))), "executable");
    }

    #! returns the lines of the output that differ from the expected output, as "expected -> actual"
    static list<string> differences(string exp, string actual) {
        list<string> el = exp.split("\n");
        list<string> al = actual.split("\n");
        list<string> rv = ();
        for (int i = 0; i < max(el.size(), al.size()); ++i) {
            if (el[i] !== al[i]) {
                rv += sprintf("%y -> %y", el[i], al[i]);
            }
        }
        return rv;
    }
}
