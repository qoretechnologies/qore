# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the cases of the CompoundAssignmentAstParity-<n>.qtest tests: the compound-assignment parity matrix, split by
# operator family and by execution mode; each test runs one part of them, so that each takes a short time, and together
# they run every case (see addPartCase())

#! Tests that every compound assignment gives the AST interpreter's result in every execution mode
/** The AST interpreter is the reference: a compound assignment operates on the lvalue's current value by its type,
    so an int lvalue converts the operand to an int (<tt>i += 1.5</tt> gives 11 for 10), a string lvalue appends the
    operand as a string, a hash lvalue merges a hash or removes keys, and so on.  The other execution modes and
    compiled code must give the same value or raise the same exception.

    The matrix is split by operator family (see OperatorFamilies), and each family is checked in four parts (the
    CompoundAssignmentAstParity-<n>.qtest tests, qore#6226): every part generates the family's program and runs it
    with the AST interpreter, whose output is the expected result, and checks one group of the other modes against
    it: the ir mode and the qcc-compiled executable, the jit mode, the tiered mode, or the default mode.  The tests
    of a family used to run every mode in one test, which took 160 s and more in a debug build (more than 300 s,
    the limit of a test, when the machine was busy); each part takes about a minute.

    The test generates a program that applies each compound operator to lvalues of many declared types and values:
    - with operands of many types whose type is not known at parse time, looping over the values at runtime, in
      locals, globals, class members, hash members and list elements
    - with literal operands, whose types the parser and the compilers use
    Statements that the parser refuses are left out.  The program runs with the AST interpreter, whose output is the
    expected result, then with every other execution mode and as a qcc-compiled executable, whose output must be
    identical.
*/
class CompoundAssignmentAstParityBase inherits QUnit::Test {
    public {
        #! the compound assignment operators, in the families that each test checks
        const OperatorFamilies = (
            ("+=",),
            ("-=",),
            ("*=",),
            ("/=",),
            ("%=", "|="),
            ("&=", "^="),
            ("<<=", ">>="),
        );

        #! the lvalue types and the initial values (as Qore source) of their lvalues
        const Types = (
            ("auto", ("10", "2.5", "2.5n", "\"s\"", "True", "2024-01-02T03:04:05Z", "1h", "(1, 2)",
                "{\"a\": 1, \"b\": 2}", "<0102>", "NOTHING", "NULL", "new C()")),
            ("int", ("10",)),
            ("*int", ("10", "NOTHING")),
            ("softint", ("10",)),
            ("*softint", ("10", "NOTHING")),
            ("softfloat", ("2.5",)),
            ("softnumber", ("2.5n",)),
            ("softstring", ("\"s\"",)),
            ("softbool", ("True",)),
            ("softdate", ("2024-01-02T03:04:05Z",)),
            ("float", ("2.5",)),
            ("*float", ("2.5", "NOTHING")),
            ("number", ("2.5n",)),
            ("*number", ("NOTHING",)),
            ("string", ("\"s\"",)),
            ("*string", ("\"s\"", "NOTHING")),
            ("bool", ("True",)),
            ("date", ("2024-01-02T03:04:05Z", "1h")),
            ("*date", ("NOTHING",)),
            ("list<auto>", ("(1, 2)",)),
            ("*list<auto>", ("NOTHING",)),
            ("hash<auto>", ("{\"a\": 1, \"b\": 2}",)),
            ("*hash<auto>", ("NOTHING",)),
            ("binary", ("<0102>",)),
            ("*binary", ("NOTHING",)),
            ("timeout", ("1000",)),
            ("*timeout", ("NOTHING",)),
            ("C", ("new C()",)),
        );

        #! the operands (as Qore source) given at runtime
        const Operands = (
            "1", "-2", "0", "0.0", "0n", "1.5", "2n", "\"3\"", "\"a\"", "True", "1h", "2024-01-01T00:00:00Z",
            "(\"a\",)", "(1, 2)", "{\"a\": 5}", "<03>", "NOTHING", "NULL", "new C()",
        );

        #! the operands given as literals
        const LiteralOperands = ("1", "0", "0.0", "1.5", "2n", "\"3\"", "1h", "2024-01-01T00:00:00Z", "NOTHING");

        #! the declarations of the generated program
        const Prelude = "%modern\n%disable-all-warnings\n"
            "class C { public { int a = 1; } }\n"
            "sub show(reference<string> out, string label, auto v) { out += sprintf(\"%s: %y %s\\n\", label, v, "
                "v.type()); }\n"
            "sub fail(reference<string> out, string label, hash<ExceptionInfo> ex) { out += sprintf(\"%s: %s\\n\", "
                "label, ex.err); }\n";

        #! the number of literal-operand statements in each generated function
        const StatementsPerFunction = 40;
    }

    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;

        #! the operators of the family that the case being run checks
        list<string> Operators;

        string qore_bin;
        string libqore_dir;
        string qcc_bin;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        qore_bin = ENV.QORE_BIN ?? ENV.QORE_BINARY ?? QORE_ARGV[0] ?? "qore";
        libqore_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);
        qcc_bin = ENV.QCC ?? ENV.QORE_QCC ?? (is_file(libqore_dir + "/qcc") ? libqore_dir + "/qcc" : "qcc");

        # the AST interpreter is the reference of every part; each other mode is checked by one part of each family
        addPartCase(1, "the parts check every execution mode", \testPartsCoverModes());
        addPartCase(1, familyName(0) + ": ir and the executable", \testParity(), (0, ("ir",), False, True));
        addPartCase(2, familyName(0) + ": jit", \testParity(), (0, ("jit",), False, False));
        addPartCase(3, familyName(0) + ": tiered", \testParity(), (0, ("tiered",), False, False));
        addPartCase(4, familyName(0) + ": the default mode", \testParity(), (0, (), True, False));
        addPartCase(5, familyName(1) + ": ir and the executable", \testParity(), (1, ("ir",), False, True));
        addPartCase(6, familyName(1) + ": jit", \testParity(), (1, ("jit",), False, False));
        addPartCase(7, familyName(1) + ": tiered", \testParity(), (1, ("tiered",), False, False));
        addPartCase(8, familyName(1) + ": the default mode", \testParity(), (1, (), True, False));
        addPartCase(9, familyName(2) + ": ir and the executable", \testParity(), (2, ("ir",), False, True));
        addPartCase(10, familyName(2) + ": jit", \testParity(), (2, ("jit",), False, False));
        addPartCase(11, familyName(2) + ": tiered", \testParity(), (2, ("tiered",), False, False));
        addPartCase(12, familyName(2) + ": the default mode", \testParity(), (2, (), True, False));
        addPartCase(13, familyName(3) + ": ir and the executable", \testParity(), (3, ("ir",), False, True));
        addPartCase(14, familyName(3) + ": jit", \testParity(), (3, ("jit",), False, False));
        addPartCase(15, familyName(3) + ": tiered", \testParity(), (3, ("tiered",), False, False));
        addPartCase(16, familyName(3) + ": the default mode", \testParity(), (3, (), True, False));
        addPartCase(17, familyName(4) + ": ir and the executable", \testParity(), (4, ("ir",), False, True));
        addPartCase(18, familyName(4) + ": jit", \testParity(), (4, ("jit",), False, False));
        addPartCase(19, familyName(4) + ": tiered", \testParity(), (4, ("tiered",), False, False));
        addPartCase(20, familyName(4) + ": the default mode", \testParity(), (4, (), True, False));
        addPartCase(21, familyName(5) + ": ir and the executable", \testParity(), (5, ("ir",), False, True));
        addPartCase(22, familyName(5) + ": jit", \testParity(), (5, ("jit",), False, False));
        addPartCase(23, familyName(5) + ": tiered", \testParity(), (5, ("tiered",), False, False));
        addPartCase(24, familyName(5) + ": the default mode", \testParity(), (5, (), True, False));
        addPartCase(25, familyName(6) + ": ir and the executable", \testParity(), (6, ("ir",), False, True));
        addPartCase(26, familyName(6) + ": jit", \testParity(), (6, ("jit",), False, False));
        addPartCase(27, familyName(6) + ": tiered", \testParity(), (6, ("tiered",), False, False));
        addPartCase(28, familyName(6) + ": the default mode", \testParity(), (6, (), True, False));

        set_return_value(main());
    }

    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! returns the name of an operator family, for the names of the cases
    private static string familyName(int family) {
        return OperatorFamilies[family].join(" ");
    }

    #! checks that the modes that the parts run are the execution modes of the harness (but ast, the reference)
    testPartsCoverModes() {
        assertEq(sort(ExecModeHarness::ExecModes), sort(("ast", "ir", "jit", "tiered")),
            "the execution modes that the parts check: update the parts when the harness has another mode");
    }

    #! runs the program of an operator family in the modes given (and in the default mode, and as a qcc-compiled
    #! executable) and checks the output against the AST interpreter's
    testParity(int family, list<string> modes, bool default_mode, bool executable) {
        Operators = OperatorFamilies[family];
        TmpDir tmp("qore-compound-ast-parity-");
        string file = tmp.path + DirSep + "parity.q";
        int combinations = writeProgram(file);

        list<string> expected = runMode("ast", file);
        # the runtime matrix gives a result for every combination
        assertTrue(combinations > 1000, sprintf("runtime combinations: %d", combinations));
        assertEq(combinations, (select expected, $1 =~ /^rt /).size(), "runtime combinations");
        assertTrue((select expected, $1 =~ /^lit /).size() > 50, "literal statements");

        foreach string mode in (modes) {
            compare(mode, expected, runMode(mode, file));
        }

        if (default_mode) {
            # the default execution mode
            compare("default", expected, ExecModeHarness::run_any(sprintf("LD_LIBRARY_PATH=%s "
                "DYLD_LIBRARY_PATH=%s %s %s", libqore_dir, libqore_dir, qore_bin, file)).split("\n"));
        }

        if (executable) {
            string exe = tmp.path + DirSep + "parity";
            string out = backquote(sprintf("QORE_LIBDIR=%s %s -o %s %s 2>&1", libqore_dir, qcc_bin, exe, file));
            assertTrue(is_file(exe), "compiled the executable: " + out);
            compare("executable", expected, ExecModeHarness::run_any(sprintf("LD_LIBRARY_PATH=%s "
                "DYLD_LIBRARY_PATH=%s %s", libqore_dir, libqore_dir, exe)).split("\n"));
        }
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

    private list<string> runMode(string mode, string file) {
        return ExecModeHarness::run(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s --exec-mode=%s %s",
            libqore_dir, libqore_dir, qore_bin, mode, file)).split("\n");
    }

    #! returns the Qore source of a list of the strings given
    private static string sourceList(list<string> l) {
        return "(" + (map sprintf("%y", $1), l).join(", ") + ",)";
    }

    #! returns the function that gives a fresh value of each initial value or operand, by index
    private static string valueFunction(string name, list<string> values) {
        string body;
        foreach string v in (values) {
            body += sprintf("        case %d: return %s;\n", $#, v);
        }
        return sprintf("auto sub %s(int i) {\n    switch (i) {\n%s    }\n}\n", name, body);
    }

    #! returns the statements that apply every operator to an lvalue in a loop over the values and operands
    /** @param label the label of the lvalue kind
        @param type the declared type
        @param values the name of the function giving the initial values
        @param count the number of initial values
        @param init the statement assigning the initial value \c "x" to the lvalue
        @param lvalue the lvalue
        @param decl the declarations that the lvalue needs
        @param combinations incremented by the number of combinations the statements apply

        An operator that the parser refuses for the lvalue with an operand of a type not known at parse time is
        left out.
    */
    private string runtimeLoop(string label, string type, string values, int count, string init, string lvalue,
            string decl, reference<int> combinations) {
        string body = "";
        foreach string op in (Operators) {
            if (!parses(sprintf("auto x = 1; %s %s %s operand(1);", init, lvalue, op), decl)) {
                continue;
            }
            combinations += count * Operands.size();
            body += sprintf("    for (int i = 0; i < %d; ++i) {\n"
                "        for (int j = 0; j < %d; ++j) {\n"
                "            string label = sprintf(\"rt %s %s %s: %%s %s %%s\", %s_src[i], operand_src[j]);\n"
                "            try {\n"
                "                auto x = %s(i);\n"
                "                %s\n"
                "                %s %s operand(j);\n"
                "                show(\\out, label, %s);\n"
                "            } catch (hash<ExceptionInfo> ex) {\n"
                "                fail(\\out, label, ex);\n"
                "            }\n"
                "        }\n"
                "    }\n", count, Operands.size(), label, type, op, op, values, values, init, lvalue, op,
                lvalue);
        }
        return body;
    }

    #! writes the program and returns the number of runtime combinations
    private int writeProgram(string file) {
        int combinations = 0;
        list<string> decls = ();
        list<string> funcs = ();

        decls += valueFunction("operand", Operands);
        decls += sprintf("const operand_src = %s;", sourceList(Operands));

        # the runtime matrix: one function for each type and lvalue kind
        foreach list<auto> t in (Types) {
            string type = t[0];
            string values = sprintf("init%d", $#);
            decls += valueFunction(values, t[1]);
            decls += sprintf("const %s_src = %s;", values, sourceList(t[1]));
            int n = t[1].size();

            string gdecl = sprintf("our %s g%d;", type, $#);
            string mdecl = sprintf("class M%d { public { %s v; } }", $#, type);
            decls += gdecl;
            decls += mdecl;

            list<list<string>> kinds = (
                ("local", sprintf("%s v = x;", type), "v", ""),
                ("global", sprintf("g%d = x;", $#), sprintf("g%d", $#), gdecl),
                ("member", sprintf("M%d m(); m.v = x;", $#), "m.v", mdecl),
            );
            if (type == "auto") {
                kinds += (("hash member", "hash<auto> h = {\"v\": x};", "h.v", ""),);
                kinds += (("list element", "list<auto> l = (x,);", "l[0]", ""),);
            }
            foreach list<string> k in (kinds) {
                string loop = runtimeLoop(k[0], type, values, n, k[1], k[2], k[3], \combinations);
                if (loop) {
                    funcs += sprintf("string sub f%d() {\n    string out;\n%s    return out;\n}\n",
                        funcs.size(), loop);
                }
            }
        }

        # the literal matrix: statements that the parser accepts
        list<string> body = ();
        foreach list<auto> t in (Types) {
            string type = t[0];
            foreach string op in (Operators) {
                foreach string init in (t[1]) {
                    foreach string operand in (LiteralOperands) {
                        string stmt = sprintf("%s v = %s; v %s %s;", type, init, op, operand);
                        if (!parses(stmt)) {
                            continue;
                        }
                        string label = sprintf("lit %s %s: %s %s %s", type, op, init, op, operand);
                        body += sprintf("    try { %s show(\\out, %y, v); } catch (hash<ExceptionInfo> ex) "
                            "{ fail(\\out, %y, ex); }", stmt, label, label);
                    }
                }
            }
        }
        # small functions keep the native compilation of each one short
        for (int i = 0; i < body.size(); i += StatementsPerFunction) {
            funcs += sprintf("string sub f%d() {\n    string out;\n%s\n    return out;\n}\n", funcs.size(),
                body[i..(i + StatementsPerFunction - 1)].join("\n"));
        }

        # the functions are called by name, so that the top-level code stays small; they run three times, so that
        # tiered execution compiles them, and the last run's results are printed
        string main = sprintf("string sub run_all() {\n    string out;\n    for (int i = 0; i < %d; ++i) {\n"
            "        out += call_function(\"f\" + i);\n    }\n    return out;\n}\n"
            "string out;\nfor (int run = 0; run < 3; ++run) {\n    out = run_all();\n}\nprint(out);\n",
            funcs.size());

        File f();
        f.open2(file, O_CREAT | O_WRONLY | O_TRUNC);
        f.write(Prelude + decls.join("\n") + "\n" + funcs.join("\n") + main);
        f.close();
        return combinations;
    }

    #! returns True if the statement parses in a function
    private bool parses(string stmt, *string decl) {
        Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
        try {
            p.parse(Prelude + "auto sub operand(int i) { return i; }\n" + (decl ?? "") + "\nsub t() { " + stmt
                + " }", "parity");
        } catch (hash<ExceptionInfo> ex) {
            return False;
        }
        return True;
    }
}
