# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# The cases of the AOTOutlineExceptionScopes-<n>.qtest tests: each compiles and runs a part of the scripts of this
# directory, so that each takes a short time, and together they run every script (see addPartCase())


#! qcc compiles the exception paths, catch blocks and handlers of functions outlined at low thresholds as the AST does
/** Each script of this directory has functions whose try statements, catch blocks and \c on_exit, \c on_error and
    \c on_success handlers end up in the outlined helpers of a function when the outliner's thresholds are forced low
    (\c QORE_AOT_OUTLINE_FN_MIN_INSTS, \c QORE_AOT_OUTLINE_FN_MIN_BLOCKS and \c QORE_AOT_OUTLINE_FN_TARGET_INSTS):
    - an exception leaving outlined code released the locals, the temporaries and the value of an interrupted return
      after the catch block or the handler instead of before (qore#6650);
    - a catch block's exception variable had no value after the exit block of a catch block in a helper closed the
      wrong catch block (qore#6651);
    - the handlers of a scope that a region opened ran when the helper returned, and a handler run in a helper
      assigned a local that the coordinator still held the old value of (qore#6652);
    - a closure created in a helper that captured locals of the region and of the coordinator could not find the
      coordinator's (qore#6654);
    - a private:internal method called from a helper was resolved to a base class's static method of the same name,
      and a local that a helper removed was taken for assigned by the coordinator (qore#6653).

    The test compiles each script at -O0 and -O3 with several region sizes and checks that the executables print what
    the AST prints.
*/
class AOTOutlineExceptionScopesBase inherits QUnit::Test {
    public {
        #! the region sizes forced
        const Targets = (4, 8, 16, 32);
        #! the qcc optimization levels
        const OptLevels = ("-O0", "-O3");

        #! the number of parts that the scripts are compiled in
        const Parts = 3;
    }

    private {
        string qore_bin;
        string qcc_bin;
        #! the library path of the qcc compiles and of the executables
        string lib_env;

        #! the part of the cases that this test runs (see addPartCase())
        int part;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        qore_bin = TestInterpreter::qore_interpreter();
        string libqore_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);
        qcc_bin = ENV.QCC ?? ENV.QORE_QCC ?? ENV.QORE_QCC_EXECUTABLE
            ?? (is_file(libqore_dir + "/qcc") ? libqore_dir + "/qcc" : dirname(qore_bin) + "/qcc");
        if (!is_file(qcc_bin)) {
            qcc_bin = "qcc";
        }
        lib_env = sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s", libqore_dir, libqore_dir);
        addPartCase(1, "exception scopes with the outliner's thresholds forced low, part 1", \testScripts(), 1);
        addPartCase(2, "exception scopes with the outliner's thresholds forced low, part 2", \testScripts(), 2);
        addPartCase(3, "exception scopes with the outliner's thresholds forced low, part 3", \testScripts(), 3);
        set_return_value(main());
    }

    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! compiles and runs the scripts of part \a n: those whose index in the sorted list is n - 1 modulo Parts
    testScripts(int n) {
        TmpDir dir("qore-aot-outline-exc-");
        list<string> all = sort(glob(get_script_dir() + "/*.q") ?? ());
        assertTrue(all.size() > 0, "the scripts are found");
        list<string> scripts = map all[$1], xrange(n - 1, all.size(), Parts);
        assertTrue(scripts.size() > 0, sprintf("part %d has scripts", n));
        foreach string src in (scripts) {
            string name = basename(src);
            # a script can restrict the region sizes with a line "# outline-targets: <n> ...", for a configuration
            # whose fault belongs to a later issue
            list<int> targets = Targets;
            *string tl = (ReadOnlyFile::readTextFile(src) =~ x/^# outline-targets:([ 0-9]+)$/m)[0];
            if (tl) {
                targets = map $1.toInt(), select tl.split(" "), $1.val();
            }
            hash<ExecModeHarness::ModeRunResult> r = ExecModeHarness::run_command(sprintf("%s -b --exec-mode=ast %s",
                qore_bin, src));
            assertEq(0, r.rc, sprintf("%s: AST exit status (output: %y)", name, r.out));
            string expected = r.stdout;
            assertTrue(expected.size() > 0, sprintf("%s: the AST prints something", name));
            foreach string opt in (OptLevels) {
                foreach int target in (targets) {
                    string what = sprintf("%s %s target %d", name, opt, target);
                    string exe = join_paths(dir.path, sprintf("%s%s-%d", name, opt, target));
                    int rc;
                    string out = backquote(sprintf("%s QORE_AOT_OUTLINE_FN_MIN_INSTS=1 QORE_AOT_OUTLINE_FN_MIN_BLOCKS=1 "
                        "QORE_AOT_OUTLINE_FN_TARGET_INSTS=%d %s %s -o %s %s 2>&1", lib_env, target, qcc_bin, opt, exe,
                        src), \rc);
                    assertEq(0, rc, sprintf("%s: qcc exit status (output: %s)", what, out.substr(-2000)));
                    if (rc) {
                        continue;
                    }
                    # -b must be the first argument of a compiled program
                    out = backquote(sprintf("%s %s -b", lib_env, exe), \rc);
                    assertEq(0, rc, sprintf("%s: exit status (output: %y)", what, out));
                    assertEq(expected, out, what);
                }
            }
        }
    }
}
