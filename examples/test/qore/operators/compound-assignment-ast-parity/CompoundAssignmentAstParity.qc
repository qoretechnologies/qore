# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the cases of the CompoundAssignmentAstParity-<n>.qtest tests: the compound-assignment parity matrix, split by
# operator family, by execution mode and by function slice; each test runs one part of them, so that each takes a short
# time, and together they run every case (see addPartCase())

#! Tests that every compound assignment gives the AST interpreter's result in every execution mode
/** The AST interpreter is the reference: a compound assignment operates on the lvalue's current value by its type,
    so an int lvalue converts the operand to an int (<tt>i += 1.5</tt> gives 11 for 10), a string lvalue appends the
    operand as a string, a hash lvalue merges a hash or removes keys, and so on.  The other execution modes and
    compiled code must give the same value or raise the same exception.

    The matrix is split by operator family (see OperatorFamilies), and each family is checked in several parts (the
    CompoundAssignmentAstParity-<n>.qtest tests, qore#6226, qore#6410).  The time of a family is in the native
    compilation of its program (33-90 functions): the jit, tiered and default runs compile every function
    synchronously (ExecModeHarness::TraceEnv; 18-50 s a run), and qcc compiles all of them (5-18 s), while the AST and
    IR interpreters take less than a second.  The first part of a family checks the AST interpreter's output of the
    whole program, runs the whole program in the ir mode and as a qcc-compiled executable, and checks that the AST
    interpreter's outputs of the slices of the functions that the other parts run, in order, are its output of the
    whole program; each other part runs a third of the functions (by the cost of their native compilation, see
    sliceBounds()) in the jit, tiered or default mode.  A slice runs a program with all
    the declarations and only its functions, checked against the AST interpreter's output of the same program; the
    functions are independent of each other, so a function gives the same output in either program.  Each part takes
    well under a minute.

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

        #! the generated programs by family, made once by generate()
        hash<string, hash<auto>> programs;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        qore_bin = TestInterpreter::qore_interpreter();
        libqore_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);
        qcc_bin = ENV.QCC ?? ENV.QORE_QCC ?? (is_file(libqore_dir + "/qcc") ? libqore_dir + "/qcc" : "qcc");

        # the AST interpreter is the reference of every part; the first part of a family runs the ir mode and the
        # executable, and each other part runs a slice of the functions in the jit, tiered or default mode
        addPartCase(1, "the parts check every execution mode", \testPartsCoverModes());
        addPartCase(1, familyName(0) + ": the AST output, ir and the executable", \testFamily(), (0, 3));
        addPartCase(2, familyName(0) + ": jit (1/3)", \testMode(), (0, "jit", 1, 3));
        addPartCase(3, familyName(0) + ": jit (2/3)", \testMode(), (0, "jit", 2, 3));
        addPartCase(4, familyName(0) + ": jit (3/3)", \testMode(), (0, "jit", 3, 3));
        addPartCase(5, familyName(0) + ": tiered (1/3)", \testMode(), (0, "tiered", 1, 3));
        addPartCase(6, familyName(0) + ": tiered (2/3)", \testMode(), (0, "tiered", 2, 3));
        addPartCase(7, familyName(0) + ": tiered (3/3)", \testMode(), (0, "tiered", 3, 3));
        addPartCase(8, familyName(0) + ": default (1/3)", \testMode(), (0, "default", 1, 3));
        addPartCase(9, familyName(0) + ": default (2/3)", \testMode(), (0, "default", 2, 3));
        addPartCase(10, familyName(0) + ": default (3/3)", \testMode(), (0, "default", 3, 3));
        addPartCase(11, familyName(1) + ": the AST output, ir and the executable", \testFamily(), (1, 3));
        addPartCase(12, familyName(1) + ": jit (1/3)", \testMode(), (1, "jit", 1, 3));
        addPartCase(13, familyName(1) + ": jit (2/3)", \testMode(), (1, "jit", 2, 3));
        addPartCase(14, familyName(1) + ": jit (3/3)", \testMode(), (1, "jit", 3, 3));
        addPartCase(15, familyName(1) + ": tiered (1/3)", \testMode(), (1, "tiered", 1, 3));
        addPartCase(16, familyName(1) + ": tiered (2/3)", \testMode(), (1, "tiered", 2, 3));
        addPartCase(17, familyName(1) + ": tiered (3/3)", \testMode(), (1, "tiered", 3, 3));
        addPartCase(18, familyName(1) + ": default (1/3)", \testMode(), (1, "default", 1, 3));
        addPartCase(19, familyName(1) + ": default (2/3)", \testMode(), (1, "default", 2, 3));
        addPartCase(20, familyName(1) + ": default (3/3)", \testMode(), (1, "default", 3, 3));
        addPartCase(21, familyName(2) + ": the AST output, ir and the executable", \testFamily(), (2, 3));
        addPartCase(22, familyName(2) + ": jit (1/3)", \testMode(), (2, "jit", 1, 3));
        addPartCase(23, familyName(2) + ": jit (2/3)", \testMode(), (2, "jit", 2, 3));
        addPartCase(24, familyName(2) + ": jit (3/3)", \testMode(), (2, "jit", 3, 3));
        addPartCase(25, familyName(2) + ": tiered (1/3)", \testMode(), (2, "tiered", 1, 3));
        addPartCase(26, familyName(2) + ": tiered (2/3)", \testMode(), (2, "tiered", 2, 3));
        addPartCase(27, familyName(2) + ": tiered (3/3)", \testMode(), (2, "tiered", 3, 3));
        addPartCase(28, familyName(2) + ": default (1/3)", \testMode(), (2, "default", 1, 3));
        addPartCase(29, familyName(2) + ": default (2/3)", \testMode(), (2, "default", 2, 3));
        addPartCase(30, familyName(2) + ": default (3/3)", \testMode(), (2, "default", 3, 3));
        addPartCase(31, familyName(3) + ": the AST output, ir and the executable", \testFamily(), (3, 3));
        addPartCase(32, familyName(3) + ": jit (1/3)", \testMode(), (3, "jit", 1, 3));
        addPartCase(33, familyName(3) + ": jit (2/3)", \testMode(), (3, "jit", 2, 3));
        addPartCase(34, familyName(3) + ": jit (3/3)", \testMode(), (3, "jit", 3, 3));
        addPartCase(35, familyName(3) + ": tiered (1/3)", \testMode(), (3, "tiered", 1, 3));
        addPartCase(36, familyName(3) + ": tiered (2/3)", \testMode(), (3, "tiered", 2, 3));
        addPartCase(37, familyName(3) + ": tiered (3/3)", \testMode(), (3, "tiered", 3, 3));
        addPartCase(38, familyName(3) + ": default (1/3)", \testMode(), (3, "default", 1, 3));
        addPartCase(39, familyName(3) + ": default (2/3)", \testMode(), (3, "default", 2, 3));
        addPartCase(40, familyName(3) + ": default (3/3)", \testMode(), (3, "default", 3, 3));
        addPartCase(41, familyName(4) + ": the AST output, ir and the executable", \testFamily(), (4, 3));
        addPartCase(42, familyName(4) + ": jit (1/3)", \testMode(), (4, "jit", 1, 3));
        addPartCase(43, familyName(4) + ": jit (2/3)", \testMode(), (4, "jit", 2, 3));
        addPartCase(44, familyName(4) + ": jit (3/3)", \testMode(), (4, "jit", 3, 3));
        addPartCase(45, familyName(4) + ": tiered (1/3)", \testMode(), (4, "tiered", 1, 3));
        addPartCase(46, familyName(4) + ": tiered (2/3)", \testMode(), (4, "tiered", 2, 3));
        addPartCase(47, familyName(4) + ": tiered (3/3)", \testMode(), (4, "tiered", 3, 3));
        addPartCase(48, familyName(4) + ": default (1/3)", \testMode(), (4, "default", 1, 3));
        addPartCase(49, familyName(4) + ": default (2/3)", \testMode(), (4, "default", 2, 3));
        addPartCase(50, familyName(4) + ": default (3/3)", \testMode(), (4, "default", 3, 3));
        addPartCase(51, familyName(5) + ": the AST output, ir and the executable", \testFamily(), (5, 3));
        addPartCase(52, familyName(5) + ": jit (1/3)", \testMode(), (5, "jit", 1, 3));
        addPartCase(53, familyName(5) + ": jit (2/3)", \testMode(), (5, "jit", 2, 3));
        addPartCase(54, familyName(5) + ": jit (3/3)", \testMode(), (5, "jit", 3, 3));
        addPartCase(55, familyName(5) + ": tiered (1/3)", \testMode(), (5, "tiered", 1, 3));
        addPartCase(56, familyName(5) + ": tiered (2/3)", \testMode(), (5, "tiered", 2, 3));
        addPartCase(57, familyName(5) + ": tiered (3/3)", \testMode(), (5, "tiered", 3, 3));
        addPartCase(58, familyName(5) + ": default (1/3)", \testMode(), (5, "default", 1, 3));
        addPartCase(59, familyName(5) + ": default (2/3)", \testMode(), (5, "default", 2, 3));
        addPartCase(60, familyName(5) + ": default (3/3)", \testMode(), (5, "default", 3, 3));
        addPartCase(61, familyName(6) + ": the AST output, ir and the executable", \testFamily(), (6, 3));
        addPartCase(62, familyName(6) + ": jit (1/3)", \testMode(), (6, "jit", 1, 3));
        addPartCase(63, familyName(6) + ": jit (2/3)", \testMode(), (6, "jit", 2, 3));
        addPartCase(64, familyName(6) + ": jit (3/3)", \testMode(), (6, "jit", 3, 3));
        addPartCase(65, familyName(6) + ": tiered (1/3)", \testMode(), (6, "tiered", 1, 3));
        addPartCase(66, familyName(6) + ": tiered (2/3)", \testMode(), (6, "tiered", 2, 3));
        addPartCase(67, familyName(6) + ": tiered (3/3)", \testMode(), (6, "tiered", 3, 3));
        addPartCase(68, familyName(6) + ": default (1/3)", \testMode(), (6, "default", 1, 3));
        addPartCase(69, familyName(6) + ": default (2/3)", \testMode(), (6, "default", 2, 3));
        addPartCase(70, familyName(6) + ": default (3/3)", \testMode(), (6, "default", 3, 3));

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

    #! checks the AST interpreter's output of the whole program of a family and its slices, and runs the whole program
    #! in the ir mode and as a qcc-compiled executable
    /** @param family the operator family
        @param slices the number of slices that the parts of the family run in the other modes
    */
    testFamily(int family, int slices) {
        TmpDir tmp("qore-compound-ast-parity-");
        string file = tmp.path + DirSep + "parity.q";
        hash<auto> p = generate(family);
        writeProgram(file, family, 0, p.funcs.size());

        list<string> expected = runMode("ast", file);
        # the runtime matrix gives a result for every combination
        assertTrue(p.combinations > 1000, sprintf("runtime combinations: %d", p.combinations));
        assertEq(p.combinations, (select expected, $1 =~ /^rt /).size(), "runtime combinations");
        assertTrue((select expected, $1 =~ /^lit /).size() > 50, "literal statements");
        # a line for each try block that the functions run
        int lines = (foldl $1 + $2, p.lines);
        assertEq(lines, expected.size(), "AST output lines");

        compare("ir", expected, runMode("ir", file));

        string exe = tmp.path + DirSep + "parity";
        string out = backquote(sprintf("QORE_LIBDIR=%s %s -o %s %s 2>&1", libqore_dir, qcc_bin, exe, file));
        assertTrue(is_file(exe), "compiled the executable: " + out);
        compare("executable", expected, ExecModeHarness::run_any(sprintf("LD_LIBRARY_PATH=%s "
            "DYLD_LIBRARY_PATH=%s %s", libqore_dir, libqore_dir, exe)).split("\n"));

        # the slices that the other parts run give the output of the whole program; each output ends with a newline,
        # which leaves no empty line in the list of its lines
        list<string> joined = ();
        for (int slice = 1; slice <= slices; ++slice) {
            list<int> b = sliceBounds(family, slice, slices);
            writeProgram(file, family, b[0], b[1]);
            joined += runMode("ast", file);
        }
        compare(sprintf("%d slices", slices), expected, joined);
    }

    #! returns the bounds of a slice of the functions of a family: the first function and the one after the last
    /** The slices have about the same cost of native compilation, which follows the number of try blocks of a
        function (qore#6392): a function of the literal matrix has 40 try blocks of one statement each, and one of the
        runtime matrix a try block in two loops for each operator, which costs about three of them

        @param family the operator family
        @param slice the slice of the functions, from 1
        @param slices the number of slices that the functions are divided into
    */
    private list<int> sliceBounds(int family, int slice, int slices) {
        list<string> funcs = generate(family).funcs;
        list<int> cumulative = ();
        int total = 0;
        foreach string func in (funcs) {
            total += (func =~ x/(try \{)/g).size() + 2 * (func =~ x/(for \(int j = 0;)/g).size();
            cumulative += total;
        }
        # the first function whose cumulative weight reaches the start of the slice and the end of the slice
        code bound = int sub (int k) {
            if (k == 0) {
                return 0;
            }
            if (k == slices) {
                return funcs.size();
            }
            for (int i = 0; i < funcs.size(); ++i) {
                if (cumulative[i] * slices >= total * k) {
                    return i + 1;
                }
            }
            return funcs.size();
        };
        int first = bound(slice - 1);
        int end = bound(slice);
        assertTrue(end > first, sprintf("slice %d of %d of %d functions: %d-%d", slice, slices, funcs.size(), first,
            end - 1));
        return (first, end);
    }

    #! runs a slice of the functions of a family in a mode and checks the output against the AST interpreter's output
    #! of the same functions
    /** @param family the operator family
        @param mode an execution mode, or \c "default" for the default execution mode
        @param slice the slice of the functions, from 1
        @param slices the number of slices that the functions are divided into
    */
    testMode(int family, string mode, int slice, int slices) {
        TmpDir tmp("qore-compound-ast-parity-");
        string file = tmp.path + DirSep + "parity.q";
        int count = generate(family).funcs.size();
        list<int> b = sliceBounds(family, slice, slices);
        int first = b[0];
        int end = b[1];
        writeProgram(file, family, first, end);
        string label = sprintf("%s (functions %d-%d of %d)", mode, first, end - 1, count);

        list<string> expected = runMode("ast", file);
        # a line for each try block that the functions of the slice run
        int lines = (foldl $1 + $2, generate(family).lines[first..end - 1]);
        assertEq(lines, expected.size(), label + ": AST output lines");
        if (mode == "default") {
            compare(label, expected, ExecModeHarness::run_any(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s %s",
                libqore_dir, libqore_dir, qore_bin, file)).split("\n"));
        } else {
            compare(label, expected, runMode(mode, file));
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

    #! generates the program of a family once and returns it
    /** @return a hash: \c decls: the declarations, \c funcs: the functions (\c f<n> is element n), \c lines: the
        number of lines that each function prints, one for each try block that it runs, \c combinations: the number of
        runtime combinations
    */
    private hash<auto> generate(int family) {
        if (programs{family}) {
            return programs{family};
        }
        Operators = OperatorFamilies[family];
        int combinations = 0;
        list<string> decls = ();
        list<string> funcs = ();
        list<int> lines = ();

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
                int before = combinations;
                string loop = runtimeLoop(k[0], type, values, n, k[1], k[2], k[3], \combinations);
                if (loop) {
                    funcs += sprintf("string sub f%d() {\n    string out;\n%s    return out;\n}\n",
                        funcs.size(), loop);
                    # a line for each combination
                    lines += combinations - before;
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
            # a line for each statement
            lines += min(StatementsPerFunction, body.size() - i);
        }

        programs{family} = {
            "decls": decls,
            "funcs": funcs,
            "lines": lines,
            "combinations": combinations,
        };
        return programs{family};
    }

    #! writes a program with the declarations and the functions f<first> to f<end - 1> of a family's program
    private writeProgram(string file, int family, int first, int end) {
        hash<auto> p = generate(family);
        # the functions are called by name, so that the top-level code stays small; they run three times, so that
        # tiered execution compiles them, and the last run's results are printed
        string main = sprintf("string sub run_all() {\n    string out;\n    for (int i = %d; i < %d; ++i) {\n"
            "        out += call_function(\"f\" + i);\n    }\n    return out;\n}\n"
            "string out;\nfor (int run = 0; run < 3; ++run) {\n    out = run_all();\n}\nprint(out);\n",
            first, end);

        File f();
        f.open2(file, O_CREAT | O_WRONLY | O_TRUNC);
        f.write(Prelude + p.decls.join("\n") + "\n" + p.funcs[first..end - 1].join("\n") + main);
        f.close();
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
