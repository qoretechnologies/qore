# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the cases of the CompoundAssignmentOrNothingPlain-<n>.qtest tests (qore#6226): each runs one part of them, so that
# each takes a short time, and together they run every case (see addPartCase())

#! Tests compound assignments on or-nothing lvalues (*T) against the plain type T, and on soft-typed lvalues
/** - an or-nothing lvalue with no value gives exactly the result of an lvalue of the plain type with no value, or
      raises the same exception, for every operand with a value
    - a soft-typed lvalue (\c softint, \c softfloat, \c softnumber, \c softstring, \c softbool, \c softdate,
      \c softlist and their or-nothing types) always holds its declared type after a compound assignment or an
      assignment, as the value is converted to its type (ex: <tt>softint v = 10; v *= 1.5</tt> leaves the int 15)

    The test generates a program applying each compound operator and the assignment operator to locals, globals and
    class members of these types with operands of many types that are not known at parse time.  The AST
    interpreter's output is checked against the rules above, and every other execution mode and a qcc-compiled
    executable must give the same output.

    The program has about 400 functions, and the time of the test is in their native compilation: the jit, tiered
    and default runs compile every function synchronously (ExecModeHarness::TraceEnv; about 0.1-0.25 s each, 50-95 s
    for a run), and qcc compiles all of them (30-45 s).  The cases are therefore in twelve parts (see addPartCase()),
    CompoundAssignmentOrNothingPlain-<n>.qtest, each of which takes well under a minute (qore#6226, qore#6360): part 1
    checks the AST interpreter's output of the whole program against the rules and runs the whole program in the ir
    mode; parts 2-3 compile half of the functions each with qcc and run the executable; parts 4-6, 7-9 and 10-12 run a
    third of the functions each in the jit, tiered and default modes.  A part that runs a slice of the functions runs
    a program with only those functions, and checks its output against the AST interpreter's output of that program;
    the functions are independent of each other, so a function gives the same output in either program, which part 1
    checks for the AST interpreter: its outputs of the slices, in order, must be its output of the whole program.
*/
class CompoundAssignmentOrNothingPlainBase inherits QUnit::Test {
    public {
        #! the operators
        const Operators = ("=", "+=", "-=", "*=", "/=", "%=", "|=", "&=", "^=", "<<=", ">>=");

        #! the plain types compared with their or-nothing types
        const PlainTypes = (
            "int", "float", "number", "string", "bool", "date", "binary", "list<auto>", "hash<auto>", "timeout",
            "softint", "softfloat", "softnumber", "softstring", "softbool", "softdate", "softlist<auto>",
        );

        #! the soft types and the initial values (as Qore source) of their lvalues with a value
        const SoftTypes = (
            ("softint", "10"),
            ("softfloat", "2.5"),
            ("softnumber", "2.5n"),
            ("softstring", "\"5\""),
            ("softbool", "True"),
            ("softdate", "2024-01-02T03:04:05Z"),
            ("softlist<auto>", "(1, 2)"),
        );

        #! the operands (as Qore source), given at runtime
        const Operands = (
            "1", "-2", "0", "0.0", "1.5", "2n", "\"3\"", "\"a\"", "True", "1h", "2024-01-01T00:00:00Z",
            "(\"a\",)", "{\"a\": 5}", "<03>", "NOTHING",
        );

        #! the type of the value that each soft type holds
        const SoftResultType = {
            "softint": "integer",
            "softfloat": "float",
            "softnumber": "number",
            "softstring": "string",
            "softbool": "bool",
            "softdate": "date",
            "softlist<auto>": "list",
        };

        #! the type of the value that each plain type holds
        const PlainResultType = {
            "int": "integer",
            "float": "float",
            "number": "number",
            "string": "string",
            "bool": "bool",
            "date": "date",
            "binary": "binary",
            "list<auto>": "list",
            "hash<auto>": "hash",
            "timeout": "integer",
        };

        #! the declarations of the generated program
        const Prelude = "%modern\n%disable-all-warnings\n"
            "sub show(reference<string> out, string label, auto v) { out += sprintf(\"%s => %y %s\\n\", label, v, "
                "v.type()); }\n"
            "sub fail(reference<string> out, string label, hash<ExceptionInfo> ex) { out += sprintf(\"%s => %s\\n\", "
                "label, ex.err); }\n";
    }

    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;

        string qore_bin;
        string libqore_dir;
        string qcc_bin;

        #! the generated program, made once by generate()
        *hash<auto> program;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        qore_bin = TestInterpreter::qore_interpreter();
        libqore_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);
        qcc_bin = ENV.QCC ?? ENV.QORE_QCC ?? (is_file(libqore_dir + "/qcc") ? libqore_dir + "/qcc" : "qcc");

        # the AST interpreter is the reference of every part; the native compilation of each other mode and of the
        # executable is shared by several parts, each of which runs a slice of the functions
        addPartCase(1, "the AST interpreter's results follow the rules", \testAstRules());
        addPartCase(1, "the parts check every execution mode", \testPartsCoverModes());
        addPartCase(1, "the slices of the program give the output of the whole program", \testSlices());
        addPartCase(1, "or-nothing lvalues give the plain type's results: ir", \testMode(), ("ir", 1, 1));
        addPartCase(2, "or-nothing lvalues give the plain type's results: the executable (1/2)", \testMode(),
            ("executable", 1, 2));
        addPartCase(3, "or-nothing lvalues give the plain type's results: the executable (2/2)", \testMode(),
            ("executable", 2, 2));
        addPartCase(4, "or-nothing lvalues give the plain type's results: jit (1/3)", \testMode(), ("jit", 1, 3));
        addPartCase(5, "or-nothing lvalues give the plain type's results: jit (2/3)", \testMode(), ("jit", 2, 3));
        addPartCase(6, "or-nothing lvalues give the plain type's results: jit (3/3)", \testMode(), ("jit", 3, 3));
        addPartCase(7, "or-nothing lvalues give the plain type's results: tiered (1/3)", \testMode(),
            ("tiered", 1, 3));
        addPartCase(8, "or-nothing lvalues give the plain type's results: tiered (2/3)", \testMode(),
            ("tiered", 2, 3));
        addPartCase(9, "or-nothing lvalues give the plain type's results: tiered (3/3)", \testMode(),
            ("tiered", 3, 3));
        addPartCase(10, "or-nothing lvalues give the plain type's results: the default mode (1/3)", \testMode(),
            ("default", 1, 3));
        addPartCase(11, "or-nothing lvalues give the plain type's results: the default mode (2/3)", \testMode(),
            ("default", 2, 3));
        addPartCase(12, "or-nothing lvalues give the plain type's results: the default mode (3/3)", \testMode(),
            ("default", 3, 3));

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

    #! checks the AST interpreter's output of the whole program against the rules
    testAstRules() {
        TmpDir tmp("qore-compound-or-nothing-plain-");
        string file = tmp.path + DirSep + "plain.q";
        hash<auto> p = generate();
        writeProgram(file, 0, p.funcs.size());
        checkAst(runMode("ast", file), p.pairs);
    }

    #! checks that the AST interpreter's outputs of the slices that the parts run, in order, are the output of the
    #! whole program, so that a function gives the same output in a slice as in the whole program
    testSlices() {
        TmpDir tmp("qore-compound-or-nothing-plain-");
        string file = tmp.path + DirSep + "plain.q";
        int count = generate().funcs.size();
        writeProgram(file, 0, count);
        # each output ends with a newline, which leaves no empty line in the list of its lines
        list<string> whole = runMode("ast", file);
        foreach int slices in ((2, 3)) {
            list<string> joined = ();
            for (int slice = 1; slice <= slices; ++slice) {
                writeProgram(file, count * (slice - 1) / slices, count * slice / slices);
                joined += runMode("ast", file);
            }
            compare(sprintf("%d slices", slices), whole, joined);
        }
    }

    #! runs a slice of the functions in a mode, or as a qcc-compiled executable, and checks the output against the
    #! AST interpreter's output of the same functions
    /** @param mode an execution mode, \c "default" for the default execution mode, or \c "executable"
        @param slice the slice of the functions, from 1
        @param slices the number of slices that the functions are divided into
    */
    testMode(string mode, int slice, int slices) {
        TmpDir tmp("qore-compound-or-nothing-plain-");
        string file = tmp.path + DirSep + "plain.q";
        int count = generate().funcs.size();
        int first = count * (slice - 1) / slices;
        int end = count * slice / slices;
        writeProgram(file, first, end);
        string label = slices > 1 ? sprintf("%s (functions %d-%d of %d)", mode, first, end - 1, count) : mode;

        list<string> expected = runMode("ast", file);
        assertTrue(expected.size() > 100, sprintf("%s: AST output: %d lines", label, expected.size()));

        switch (mode) {
            case "default":
                compare(label, expected, ExecModeHarness::run_any(sprintf("LD_LIBRARY_PATH=%s "
                    "DYLD_LIBRARY_PATH=%s %s %s", libqore_dir, libqore_dir, qore_bin, file)).split("\n"));
                break;

            case "executable": {
                string exe = tmp.path + DirSep + "plain";
                string out = backquote(sprintf("QORE_LIBDIR=%s %s -o %s %s 2>&1", libqore_dir, qcc_bin, exe,
                    file));
                assertTrue(is_file(exe), "compiled the executable: " + out);
                compare(label, expected, ExecModeHarness::run_any(sprintf("LD_LIBRARY_PATH=%s "
                    "DYLD_LIBRARY_PATH=%s %s", libqore_dir, libqore_dir, exe)).split("\n"));
                break;
            }

            default:
                compare(label, expected, runMode(mode, file));
                break;
        }
    }

    #! checks the AST interpreter's results against the rules
    private checkAst(list<string> lines, int pairs) {
        # the plain lvalue's results, by key
        hash<string, string> plain = {};
        hash<string, string> or_nothing = {};
        list<string> soft = ();
        foreach string line in (lines) {
            *list<*string> m = (line =~ x/^(plain|or-nothing|soft) (.+) => (.*)$/);
            if (!m) {
                continue;
            }
            switch (m[0]) {
                case "plain": plain{m[1]} = m[2]; break;
                case "or-nothing": or_nothing{m[1]} = m[2]; break;
                case "soft": soft += line; break;
            }
        }
        assertEq(pairs, plain.size(), "plain results");
        assertEq(pairs, or_nothing.size(), "or-nothing results");

        list<string> diffs = ();
        list<string> plain_type_violations = ();
        foreach hash<auto> i in (plain.pairIterator()) {
            # key: kind type op operand
            *list<*string> k = (i.key =~ x/^[^ ]+ [^ ]+ ([^ ]+) (.*)$/);
            # with an operand with no value, the AST interpreter's result for the or-nothing type is the reference
            if (k[1] == "NOTHING") {
                continue;
            }
            # a plain lvalue holds only its declared type, or no value (ex: a hash after -=)
            *list<*string> pk = (i.key =~ x/^[^ ]+ ([^ ]+) /);
            *list<*string> pv = (i.value =~ x/ ([a-z]+)$/);
            if (pv && pv[0] != "nothing" && PlainResultType{pk[0]} && PlainResultType{pk[0]} != pv[0]) {
                plain_type_violations += i.key + ": " + i.value;
            }
            string expected = i.value;
            if (or_nothing{i.key} != expected) {
                diffs += sprintf("%s: plain: %s or-nothing: %s", i.key, i.value, or_nothing{i.key});
            }
        }
        assertEq((), diffs, sprintf("%d or-nothing results differ from the plain type's", diffs.size()));
        assertEq((), plain_type_violations, "plain lvalues hold their declared type");

        # a soft lvalue holds its declared type, or no value for an or-nothing type
        list<string> wrong = ();
        foreach string line in (soft) {
            *list<*string> m = (line =~ x/^soft [^ ]+ ([^ ]+) .* => .* ([a-z]+)$/);
            if (!m) {
                # an exception
                continue;
            }
            bool on = m[0] =~ /^\*/;
            string expected = SoftResultType{on ? m[0].substr(1) : m[0]};
            if (m[1] != expected && !(on && m[1] == "nothing")) {
                wrong += line;
            }
        }
        assertEq((), wrong, "soft lvalues hold their declared type");
        assertTrue(soft.size() > 500, sprintf("soft results: %d", soft.size()));
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

    #! returns the statements applying the operator in a loop over the operands
    /** @param label the label of the result
        @param decl the declaration of the lvalue with its initial value, if any
        @param lvalue the lvalue
        @param op the operator
    */
    private static string loop(string label, string decl, string lvalue, string op) {
        return sprintf("    for (int j = 0; j < %d; ++j) {\n"
            "        string label = sprintf(\"%s %%s\", operand_src[j]);\n"
            "        try {\n"
            "            %s\n"
            "            %s %s operand(j);\n"
            "            show(\\out, label, %s);\n"
            "        } catch (hash<ExceptionInfo> ex) {\n"
            "            fail(\\out, label, ex);\n"
            "        }\n"
            "    }\n", Operands.size(), label, decl, lvalue, op, lvalue);
    }

    #! generates the program once and returns it
    /** @return a hash: \c decls: the declarations, \c funcs: the functions (\c f<n> is element n), \c pairs: the
        number of plain and or-nothing result pairs
    */
    private hash<auto> generate() {
        if (program) {
            return program;
        }
        list<string> decls = ();
        list<string> funcs = ();
        int pairs = 0;

        string body;
        foreach string v in (Operands) {
            body += sprintf("        case %d: return %s;\n", $#, v);
        }
        decls += sprintf("auto sub operand(int i) {\n    switch (i) {\n%s    }\n}\n", body);
        decls += sprintf("const operand_src = %s;", sourceList(Operands));

        int n = 0;
        # lvalue kinds; arguments: the declaration name suffix
        code kinds = list<list<string>> sub (string type, *string init) {
            ++n;
            string iv = init ? " = " + init : "";
            # an lvalue without an initial value is left with no value
            string g = init ? sprintf("g%d = %s;", n, init) : sprintf("remove g%d;", n);
            string mi = init ? sprintf(" m.v = %s;", init) : "";
            return (
                ("local", sprintf("%s v%s;", type, iv), "v", ""),
                ("global", g, sprintf("g%d", n), sprintf("our %s g%d;", type, n)),
                ("member", sprintf("M%d m();%s", n, mi), "m.v", sprintf("class M%d { public { %s v; } }", n, type)),
            );
        };

        # or-nothing lvalues with no value against the plain type
        foreach string type in (PlainTypes) {
            list<list<string>> pk = kinds(type);
            list<list<string>> ok = kinds("*" + type);
            for (int i = 0; i < pk.size(); ++i) {
                decls += pk[i][3];
                decls += ok[i][3];
                foreach string op in (Operators) {
                    bool pp = parses(sprintf("%s %s %s operand(1);", pk[i][1], pk[i][2], op), pk[i][3]);
                    bool op_parses = parses(sprintf("%s %s %s operand(1);", ok[i][1], ok[i][2], op), ok[i][3]);
                    # the parser refuses the same statements for both types
                    assertEq(pp, op_parses, sprintf("%s %s %s parses", pk[i][0], type, op));
                    if (!pp || !op_parses) {
                        continue;
                    }
                    pairs += Operands.size();
                    funcs += sprintf("string sub f%d() {\n    string out;\n%s%s    return out;\n}\n", funcs.size(),
                        loop(sprintf("plain %s %s %s", pk[i][0], type, op), pk[i][1], pk[i][2], op),
                        loop(sprintf("or-nothing %s %s %s", ok[i][0], type, op), ok[i][1], ok[i][2], op));
                }
            }
        }

        # soft lvalues with a value
        foreach list<string> t in (SoftTypes) {
            foreach string type in ((t[0], "*" + t[0])) {
                list<list<string>> sk = kinds(type, t[1]);
                foreach list<string> k in (sk) {
                    decls += k[3];
                    foreach string op in (Operators) {
                        if (!parses(sprintf("%s %s %s operand(1);", k[1], k[2], op), k[3])) {
                            continue;
                        }
                        funcs += sprintf("string sub f%d() {\n    string out;\n%s    return out;\n}\n",
                            funcs.size(), loop(sprintf("soft %s %s %s", k[0], type, op), k[1], k[2], op));
                    }
                }
            }
        }

        program = {
            "decls": decls,
            "funcs": funcs,
            "pairs": pairs,
        };
        return program;
    }

    #! writes a program with the declarations and the functions f<first> to f<end - 1> of the generated program
    private writeProgram(string file, int first, int end) {
        # the functions are called by name, so that the top-level code stays small; they run three times, so that
        # tiered execution compiles them, and the last run's results are printed
        string main = sprintf("string sub run_all() {\n    string out;\n    for (int i = %d; i < %d; ++i) {\n"
            "        out += call_function(\"f\" + i);\n    }\n    return out;\n}\n"
            "string out;\nfor (int run = 0; run < 3; ++run) {\n    out = run_all();\n}\nprint(out);\n",
            first, end);

        hash<auto> p = generate();
        File f();
        f.open2(file, O_CREAT | O_WRONLY | O_TRUNC);
        f.write(Prelude + (select p.decls, $1).join("\n") + "\n" + p.funcs[first..end - 1].join("\n") + main);
        f.close();
    }

    #! returns True if the statement parses in a function
    private bool parses(string stmt, string decl) {
        Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
        try {
            p.parse(Prelude + "auto sub operand(int i) { return i; }\n" + decl + "\nsub t() { " + stmt + " }",
                "plain");
        } catch (hash<ExceptionInfo> ex) {
            return False;
        }
        return True;
    }
}
