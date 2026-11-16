# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the cases of the HashValueIncrementExecModes-<n>.qtest tests (qore#6226): each runs one part of them, so that each takes a short time, and
# together they run every case (see addPartCase())

#! Tests ++ and -- on the values of typed hashes in every execution mode against the AST interpreter
/** The IR, JIT and tiered execution modes and compiled (AOT) code applied ++ and -- to a value of a typed hash in a
    local variable with an integer operation, whatever the hash's value type: on a <tt>hash<string, float></tt>
    holding 2.5, <tt>++h.v</tt> gave the integer 3 and stored 3.0, where the AST interpreter gives and stores 3.5.
    The same happened with number, *float and softfloat values, and with a key given by an expression.  Only a hash
    of integers now takes the integer operation; any other value type gets the AST operators' result.

    The test generates a program applying the operators to such values and prints the result and the stored value
    with their types.  The AST interpreter's output is the expected result; every other execution mode, the default
    execution mode and a qcc-compiled executable must give the same output.

    The cases are in four parts (see addPartCase()), HashValueIncrementExecModes-<n>.qtest, so that each runs for a short time (about 31 s for each of the jit, tiered and default runs and 18 s for the qcc compile in a debug build; the whole test took 108 s alone and 260 s when the machine was busy,
    qore#6226): every part generates the program and runs it with the AST interpreter, whose output is the expected
    result, and part 1 also checks the ir mode and the qcc-compiled executable, part 2 the jit mode, part 3 the tiered
    mode and part 4 the default mode.
*/
class HashValueIncrementExecModesBase inherits QUnit::Test {
    public {
        #! the hash value types
        const ValueTypes = (
            "int", "*int", "float", "*float", "number", "*number", "softint", "softfloat", "softnumber", "auto",
        );

        #! the values (as Qore source) that a key holds before the operation; NOTHING: the key does not exist
        const Values = ("2", "2.5", "-0.5", "2.5n", "NOTHING");

        #! the operators: the format of the expression for an lvalue
        const Operators = ("++%s", "--%s", "%s++", "%s--");
    }

    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;

        string qore_bin;
        string libqore_dir;
        string qcc_bin;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        qore_bin = ENV.QORE_BIN ?? ENV.QORE_BINARY ?? QORE_ARGV[0] ?? "qore";
        libqore_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);
        qcc_bin = ENV.QCC ?? ENV.QORE_QCC ?? (is_file(libqore_dir + "/qcc") ? libqore_dir + "/qcc" : "qcc");

        # the AST interpreter is the reference of every part; each other mode is checked by one part
        addPartCase(1, "every execution mode gives the AST result: ir and the executable", \testMatrix(), (("ir",), False, True));
        addPartCase(1, "the parts check every execution mode", \testPartsCoverModes());
        addPartCase(2, "every execution mode gives the AST result: jit", \testMatrix(), (("jit",), False, False));
        addPartCase(3, "every execution mode gives the AST result: tiered", \testMatrix(), (("tiered",), False, False));
        addPartCase(4, "every execution mode gives the AST result: the default mode", \testMatrix(), ((), True, False));

        set_return_value(main());
    }

    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! checks that the modes that the parts run are the execution modes of the harness (but ast, the reference)
    testPartsCoverModes() {
        assertEq(sort(ExecModeHarness::ExecModes), sort(("ast", "ir", "jit", "tiered")),
            "the execution modes that the parts check: update the parts when the harness has another mode");
    }

    #! runs the program in the modes given (and in the default mode, and as a qcc-compiled executable) and checks the
    #! output against the AST interpreter's
    testMatrix(list<string> modes, bool default_mode, bool executable) {
        TmpDir tmp("qore-hash-value-increment-");
        string file = tmp.path + DirSep + "increment.q";
        int rows = writeProgram(file);

        list<string> expected = run(sprintf("--exec-mode=ast %s", file));
        assertEq(rows, (select expected, $1 =~ / => /).size(), "every row gave a result");
        # the rows that the integer operation got wrong must be among them
        assertEq(1, (select expected, $1 == "local|float|2.5|++%s|const => 3.5 float | 3.5 float").size());
        assertEq(1, (select expected, $1 == "local|number|2.5n|--%s|dyn => 1.5 number | 1.5 number").size());

        foreach string mode in (modes) {
            compare(mode, expected, run(sprintf("--exec-mode=%s %s", mode, file)));
        }

        if (default_mode) {
            # the default execution mode
            compare("default", expected, ExecModeHarness::run_any(sprintf("LD_LIBRARY_PATH=%s "
                "DYLD_LIBRARY_PATH=%s %s %s", libqore_dir, libqore_dir, qore_bin, file)).split("\n"));
        }

        if (executable) {
            string exe = tmp.path + DirSep + "increment";
            string out = backquote(sprintf("QORE_LIBDIR=%s %s -o %s %s 2>&1", libqore_dir, qcc_bin, exe, file));
            assertTrue(is_file(exe), "compiled the executable: " + out);
            compare("executable", expected, ExecModeHarness::run_any(sprintf("LD_LIBRARY_PATH=%s "
                "DYLD_LIBRARY_PATH=%s %s", libqore_dir, libqore_dir, exe)).split("\n"));
        }
    }

    private list<string> run(string args) {
        return ExecModeHarness::run(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s %s", libqore_dir,
            libqore_dir, qore_bin, args)).split("\n");
    }

    #! compares the lines of a mode with the AST lines, reporting the first differences
    private compare(string mode, list<string> expected, list<string> actual) {
        list<string> diffs = ();
        int count = 0;
        int n = max(expected.size(), actual.size());
        for (int i = 0; i < n; ++i) {
            if (expected[i] !== actual[i]) {
                ++count;
                if (diffs.size() < 20) {
                    diffs += sprintf("ast: %y\n%s: %y", expected[i], mode, actual[i]);
                }
            }
        }
        assertEq((), diffs, sprintf("%s: %d differences:\n%s", mode, count, diffs.join("\n")));
    }

    #! writes the program and returns the number of rows it prints
    private int writeProgram(string file) {
        string src = "%modern\n%disable-all-warnings\n"
            "sub show(string label, auto r, auto v) { printf(\"%s => %y %s | %y %s\\n\", label, r, r.type(), v, "
                "v.type()); }\n"
            "sub fail(string label, hash<ExceptionInfo> ex) { printf(\"%s => EXC %s\\n\", label, ex.err); }\n"
            # gives the value at runtime, so that the parser does not know its type
            "auto sub id(auto v) { return v; }\n";
        list<string> calls = ();
        int rows = 0;
        foreach string vtype in (ValueTypes) {
            foreach string val in (Values) {
                foreach string opfmt in (Operators) {
                    foreach string keykind in (("const", "dyn")) {
                        string label = sprintf("local|%s|%s|%s|%s", vtype, val, opfmt, keykind);
                        string fn = sprintf("c%d", rows);
                        string lv = keykind == "const" ? "h.v" : "h{k}";
                        # the hash is declared with its type so that the parser knows its value type; the value is
                        # assigned at runtime, which converts it as the value type does, or refuses it
                        string init = val == "NOTHING" ? "" : sprintf("        h.v = id(%s);\n", val);
                        src += sprintf("sub %s() {\n    string k = \"v\";\n    try {\n"
                            "        hash<string, %s> h = {};\n%s"
                            "        auto r = %s;\n        show(%y, r, h{k});\n"
                            "    } catch (hash<ExceptionInfo> ex) {\n        fail(%y, ex);\n    }\n}\n",
                            fn, vtype, init, sprintf(opfmt, lv), label, label);
                        calls += fn + "();";
                        ++rows;
                    }
                }
            }
        }
        src += calls.join("\n") + "\n";
        File f();
        f.open2(file, O_CREAT | O_WRONLY | O_TRUNC);
        f.write(src);
        return rows;
    }
}
