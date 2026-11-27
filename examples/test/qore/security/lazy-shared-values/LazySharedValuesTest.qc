# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The cases of the LazySharedValuesTest-<n>.qtest tests: each runs one part of them, so that each takes a short time,
# and together they run every case (see addPartCase())

#! Cases of the regression test for qore#7684: process-wide values built on first use are built as trusted code
/** LazySharedValues.q uses a library module that builds process-wide values with LazySharedMap and LazySharedValue,
    and with the plain lazy pattern that they replace: only trusted code creates the objects; for each state of the
    first caller that the barrier of a trusted build hides (a sandbox, a sandbox that a trusted deputy acts for, a
    hidden sandboxed caller record, a restricted Program with no sandbox) the value's objects are marked as shared and
    its code records no restrictions, where the plain pattern gives unmarked objects and restricted code, and a plain
    trusted first caller gets the same without the barrier; several first callers share one build, a caller with a
    timeout gives up; a failed build, an unknown key, a builder that needs its own value, a cycle of waits of
    builders and a value that a tenant created leave no entry; a map's maximum size is enforced.  It uses the debug
    hooks of debug builds of the library, so its cases are skipped in other builds.
    LazySharedVendoredSchemas.q checks that the data provider modules that load their vendored schemas on first use
    build them for a sandboxed first caller as trusted code.  Their output must be the one in the .expected files in
    every execution mode, and in programs compiled with qcc -O0 and -O3 (the callers' code is parsed at run time, so
    the compiled code there is that of the trusted callers and of the test module).

    The cases are in three parts, one test each, so that none takes long (LazySharedVendoredSchemas.q builds the
    schemas of the provider modules in each run, about 9 s in a debug build, and all the cases took 120-156 s in the
    Alpine CI jobs): LazySharedValuesTest-1.qtest runs LazySharedVendoredSchemas.q in every execution mode,
    LazySharedValuesTest-2.qtest as programs compiled with qcc, and LazySharedValuesTest-3.qtest runs
    LazySharedValues.q and LazySharedModuleByPath.q in every execution mode and compiled with qcc.
*/
class LazySharedValuesTestBase inherits QUnit::Test {
    private {
        string qore_bin = TestInterpreter::qore_interpreter();

        #! the part of the cases that this test runs (see addPartCase())
        int part;
    }

    public {
        #! the fixtures that need the debug hooks of debug builds of the library
        const DebugHookFixtures = {"LazySharedValues": True};
    }

    constructor(string name, int part) : QUnit::Test("lazy shared values", "1.0", \ARGV) {
        self.part = part;

        # the fixture that loads the vendored schema providers takes most of the time (about 9 s per run in a
        # debug build), so its execution modes and its qcc programs are parts of their own
        foreach string mode in (ExecModeHarness::ExecModes) {
            addPartCase(1, "LazySharedVendoredSchemas: execution mode " + mode,
                modeCase("LazySharedVendoredSchemas", mode));
        }
        addPartCase(2, "LazySharedVendoredSchemas: qcc -O0", qccCase("LazySharedVendoredSchemas", 0));
        addPartCase(2, "LazySharedVendoredSchemas: qcc -O3", qccCase("LazySharedVendoredSchemas", 3));

        foreach string fixture in (("LazySharedValues", "LazySharedModuleByPath")) {
            foreach string mode in (ExecModeHarness::ExecModes) {
                addPartCase(3, fixture + ": execution mode " + mode, modeCase(fixture, mode));
            }
            addPartCase(3, fixture + ": qcc -O0", qccCase(fixture, 0));
            addPartCase(3, fixture + ": qcc -O3", qccCase(fixture, 3));
        }

        set_return_value(main());
    }

    #! adds a case if it is in the part that this test runs: each case is in one part, and each part is a test
    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! returns the case that runs the fixture in an execution mode and compares its output
    private code modeCase(string name, string execution_mode) {
        return sub () {
            skipWithoutDebugHooks(name);
            # the library modules' code chooses its own execution, so the native compilation is not checked
            assertEq(expected(name), ExecModeHarness::run_stdout(command(name, execution_mode),
                ExecModeHarness::NativeCheck::None));
        };
    }

    #! returns the case that compiles the fixture with qcc and compares the output of the program
    private code qccCase(string name, int opt) {
        return sub () {
            skipWithoutDebugHooks(name);
            checkQcc(name, opt);
        };
    }

    #! skips a case of a fixture that needs the debug hooks of debug builds of the library, in other builds
    private skipWithoutDebugHooks(string name) {
        if (DebugHookFixtures{name} && !Option::HAVE_LIBRARY_DEBUGGING) {
            testSkip(sprintf("the library is not a debug build: %s.q needs its debug hooks (dbg_*())", name));
        }
    }

    private string fixture(string name) {
        return get_script_dir() + "/" + name + ".q";
    }

    private string expected(string name) {
        return ReadOnlyFile::readTextFile(get_script_dir() + "/" + name + ".expected");
    }

    private string command(string name, string mode) {
        return sprintf("%s --enable-debug --exec-mode=%s %s", shellQuote(qore_bin), mode, shellQuote(fixture(name)));
    }

    #! compiles the fixture with qcc and compares the output of the program
    private checkQcc(string name, int opt) {
        string qcc_bin = ENV.QCC_BIN ?? dirname(qore_bin) + "/qcc";
        if (!is_file(qcc_bin)) {
            qcc_bin = "qcc";
        }
        string dir = make_tmp_dir();
        on_exit remove_tree(dir);
        string program = dir + "/" + name;
        string output = backquote(sprintf("%s -O%d -o %s %s 2>&1", shellQuote(qcc_bin), opt, shellQuote(program),
            shellQuote(fixture(name))));
        assertTrue(is_file(program), "the fixture compiled: " + output);
        # the output of the fixture only: a library module that the program loads from source because its binary
        # module was built for another Qore library warns on stderr
        assertEq(expected(name), backquote(shellQuote(program) + " 2>/dev/null"));
    }

    private static string shellQuote(string value) {
        return "'" + replace(value, "'", "'\\''") + "'";
    }
}
