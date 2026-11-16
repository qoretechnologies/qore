# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the cases of the CompoundAssignmentUnassigned-<n>.qtest tests (qore#6226): each runs one part of them, so that each takes a short time, and
# together they run every case (see addPartCase())

#! Tests =, += and -= on unassigned lvalues in every execution mode against the AST interpreter
/** An unassigned lvalue of a type T or *T (a local, a hash member or an object member with no value) gets the
    value that the AST interpreter gives it from =, += and -=, for every type and every kind of operand, including
    an operand with no value: = always assigns it, += assigns it, and -= assigns an lvalue of an arithmetic type but
    leaves a hash, an object or a list unassigned when it removes keys (ex: <tt>int s; s -= 0</tt> gives 0, while
    <tt>hash<auto> h; h -= "x"</tt> leaves \c h with no value).

    The test generates a program applying the operators to such lvalues and prints the value and type of the lvalue
    afterwards, and for a hash member whether the hash has the key.  The AST interpreter's output is the expected
    result; every other execution mode, the default execution mode and a qcc-compiled executable must give the same
    output.

    The cases are in four parts (see addPartCase()), CompoundAssignmentUnassigned-<n>.qtest, so that each runs for a short time (about 22 s for each of the jit, tiered and default runs and 15 s for the qcc compile in a debug build; the whole test took 85 s alone and 211 s when the machine was busy,
    qore#6226): every part generates the program and runs it with the AST interpreter, whose output is the expected
    result, and part 1 also checks the ir mode and the qcc-compiled executable, part 2 the jit mode, part 3 the tiered
    mode and part 4 the default mode.
*/
class CompoundAssignmentUnassignedBase inherits QUnit::Test {
    public {
        #! the operators
        const Operators = ("=", "+=", "-=");

        #! the plain types; each is also checked as an or-nothing type
        const Types = (
            "int", "float", "number", "date", "string", "bool", "binary", "hash<auto>", "hash<H>", "C",
            "list<auto>", "timeout", "softint", "softstring",
        );

        #! the operands (as Qore source), given at runtime
        const Operands = (
            "NOTHING", "0", "1", "1.5", "2n", "\"x\"", "True", "1h", "2024-01-01T00:00:00Z", "(\"x\",)", "{\"x\": 1}",
            "<01>", "new C()",
        );

        #! the declarations of the generated program
        const Prelude = "%modern\n%disable-all-warnings\n"
            "class C { public { int x = 1; } }\n"
            "hashdecl H { int x = 1; string y = \"y\"; }\n"
            "sub show(reference<string> out, string label, auto v, *bool key) { out += sprintf(\"%s => %y %s%s\\n\", "
                "label, v, v.type(), exists key ? sprintf(\" key=%y\", key) : \"\"); }\n"
            "sub fail(reference<string> out, string label, hash<ExceptionInfo> ex) { out += sprintf(\"%s => %s\\n\", "
                "label, ex.err); }\n";
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
        qore_bin = TestInterpreter::qore_interpreter();
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
        TmpDir tmp("qore-compound-unassigned-");
        string file = tmp.path + DirSep + "unassigned.q";
        int rows = writeProgram(file);

        list<string> expected = run(sprintf("--exec-mode=ast %s", file));
        assertEq(rows, (select expected, $1 =~ / => /).size(), "every row gave a result");
        assertTrue(rows > 1000, sprintf("rows: %d", rows));

        foreach string mode in (modes) {
            compare(mode, expected, run(sprintf("--exec-mode=%s %s", mode, file)));
        }

        if (default_mode) {
            # the default execution mode
            compare("default", expected, ExecModeHarness::run_any(sprintf("LD_LIBRARY_PATH=%s "
                "DYLD_LIBRARY_PATH=%s %s %s", libqore_dir, libqore_dir, qore_bin, file)).split("\n"));
        }

        if (executable) {
            string exe = tmp.path + DirSep + "unassigned";
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

    #! returns the Qore source of a list of the strings given
    private static string sourceList(list<string> l) {
        return "(" + (map sprintf("%y", $1), l).join(", ") + ",)";
    }

    #! writes the program and returns the number of rows it prints
    private int writeProgram(string file) {
        list<string> decls = ();
        list<string> funcs = ();
        int rows = 0;

        string body;
        foreach string v in (Operands) {
            body += sprintf("        case %d: return %s;\n", $#, v);
        }
        decls += sprintf("auto sub operand(int i) {\n    switch (i) {\n%s    }\n}\n", body);
        decls += sprintf("const operand_src = %s;", sourceList(Operands));

        int n = 0;
        foreach string plain in (Types) {
            foreach string type in ((plain, "*" + plain)) {
                ++n;
                string mdecl = sprintf("class M%d { public { %s v; } }", n, type);
                decls += mdecl;
                # kind, declaration, lvalue, result expression, key expression
                list<list<string>> kinds = (
                    ("local", sprintf("%s v;", type), "v", "v", "NOTHING"),
                    ("hash member", sprintf("hash<string, %s> h = {};", type), "h.v", "h.v", "h.hasKey(\"v\")"),
                    ("object member", sprintf("M%d m();", n), "m.v", "m.v", "NOTHING"),
                );
                foreach list<string> k in (kinds) {
                    foreach string op in (Operators) {
                        if (!parses(sprintf("%s %s %s operand(1);", k[1], k[2], op), mdecl)) {
                            continue;
                        }
                        rows += Operands.size();
                        funcs += sprintf("string sub f%d() {\n    string out;\n"
                            "    for (int j = 0; j < %d; ++j) {\n"
                            "        string label = sprintf(\"%s %s %s %%s\", operand_src[j]);\n"
                            "        try {\n"
                            "            %s\n"
                            "            %s %s operand(j);\n"
                            "            show(\\out, label, %s, %s);\n"
                            "        } catch (hash<ExceptionInfo> ex) {\n"
                            "            fail(\\out, label, ex);\n"
                            "        }\n"
                            "    }\n"
                            "    return out;\n}\n", funcs.size(), Operands.size(), k[0], type, op, k[1], k[2], op,
                            k[3], k[4]);
                    }
                }
            }
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
        return rows;
    }

    #! returns True if the statement parses in a function
    private bool parses(string stmt, string decl) {
        Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
        try {
            p.parse(Prelude + "auto sub operand(int i) { return i; }\n" + decl + "\nsub t() { " + stmt + " }",
                "unassigned");
        } catch (hash<ExceptionInfo> ex) {
            return False;
        }
        return True;
    }
}
