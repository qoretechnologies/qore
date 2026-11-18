# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the cases of the CompoundAssignmentMatrix-<n>.qtest tests (qore#6391): each runs one part of them, so that each takes
# a short time, and together they run every case (see addPartCase())

#! Regression test: every compound assignment operator gives the AST interpreter's result and value
/** The script applies each of the ten compound assignment operators (<tt>+= -= *= /= %= &= |= ^= <<= >>=</tt>) to
    lvalues of each kind (local, global and thread_local variables, members, hash and list elements and references)
    and of each type (\c int, \c softint, \c float, \c number, \c auto, \c *softint), with typed operands
    (\c int, \c float, \c number, \c string) and
    \c auto operands holding a number, a float and no value, and prints the result of the operator and the value of
    the lvalue afterwards, with their types, or the error raised.  A combination whose code does not parse without a
    warning (ex: a \c float operand of *= on an \c int lvalue) is left out; the \c auto operands cover it at runtime.

    The expected output is the AST interpreter's, computed when the test runs (it has some 1600 lines).  It is
    checked first: every result must be the value that the lvalue holds afterwards, as for an assignment (x *= y gives
    what x = x * y gives; the AST interpreter's *= and /= gave the computed float, ex: \c 17.5 for a \c softint
    lvalue holding \c 17), and the pinned lines check results that the compiled modes got wrong (*= and /= on a
    \c softint or \c *softint local or thread_local variable with a number operand) and controls.  The script runs
    in every execution mode, on the IR tier of tiered execution and as a qcc executable (see ExecModeOutputTest).

    The time of the test is in the native compilation of the script (41 functions of some 40 compound assignments
    in try blocks each, qore#6392): the jit and tiered runs compile every function synchronously (about 35-70 s
    each), and qcc compiles all of them (30-50 s), while the other runs take about a second.  The cases are
    therefore in nine parts (see addPartCase()), CompoundAssignmentMatrix-<n>.qtest, each of which takes well
    under a minute (qore#6391): part 1 checks the AST interpreter's output of the whole script (the pinned results
    and the rules) and runs the whole script with the AST and IR interpreters and on the IR tier of tiered
    execution; parts 2-4 and 5-7 run a third of the functions each with JIT and tiered execution, and parts 8-9
    compile half of the functions each with qcc and run the executable.  A part that runs a slice of the functions
    runs a script with only those functions and their calls, and checks its output against the AST interpreter's
    output of that script; the functions are independent of each other, so a function gives the same output in
    either script, which part 1 checks: the AST interpreter's outputs of the slices, in order, must be the output of
    the whole script.
*/
class CompoundAssignmentMatrixBase inherits QUnit::Test {
    public {
        #! the operators
        const Ops = ("+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=");

        #! the lvalue types and initial values
        const Types = (
            ("int", "7"),
            ("softint", "7"),
            ("float", "7.0"),
            ("number", "7n"),
            ("auto", "7"),
            ("*softint", "7"),
        );

        #! the operand types and values
        const Operands = (
            ("int", "2"),
            ("float", "2.5"),
            ("number", "2.5n"),
            ("string", "\"3\""),
            ("auto", "2.5n"),
            ("auto", "2.5"),
            ("auto", "NOTHING"),
        );

        #! the lvalue kinds
        /** local and thread_local variables are changed by loading, computing and storing, the other kinds through
            their lvalue path, like static class variables and closure-bound locals, which are left out to keep the
            native compilation of the script short
        */
        const Kinds = ("local", "global", "tl", "member", "hash", "list", "ref");

        #! what every mode and the executable must give
        const Desc = "gives the result and value of every compound assignment that the AST interpreter gives";

        #! results of the AST interpreter that the output must hold
        const Pinned = (
            "local softint auto:2.5n *=: integer:17 integer:17",
            "local softint auto:2.5n /=: integer:2 integer:2",
            "local *softint auto:2.5n *=: integer:17 integer:17",
            "local *softint auto:2.5n /=: integer:2 integer:2",
            "tl softint auto:2.5n *=: integer:17 integer:17",
            "tl softint auto:2.5n /=: integer:2 integer:2",
            "tl *softint auto:2.5n *=: integer:17 integer:17",
            "ref softint auto:2.5n *=: integer:17 integer:17",
            "global softint auto:2.5n *=: integer:17 integer:17",
            "local softint auto:2.5n +=: integer:9 integer:9",
            "local softint auto:2.5 *=: integer:17 integer:17",
            "local softint auto:2.5 /=: integer:2 integer:2",
            "member softint auto:2.5 *=: integer:17 integer:17",
            "hash softint auto:2.5 /=: integer:2 integer:2",
            "ref *softint auto:2.5 *=: integer:17 integer:17",
            "local auto auto:2.5n *=: number:17.5 number:17.5",
            "local auto auto:2.5 /=: float:2.8 float:2.8",
            "local int int:2 <<=: integer:28 integer:28",
        );
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
        qcc_bin = ENV.QCC ?? ENV.QORE_QCC ?? ENV.QORE_QCC_EXECUTABLE
            ?? (is_file(libqore_dir + "/qcc") ? libqore_dir + "/qcc" : dirname(qore_bin) + "/qcc");
        if (!is_file(qcc_bin)) {
            qcc_bin = "qcc";
        }

        # the AST interpreter's output is the reference of every part; the native compilation of the jit and tiered
        # modes and of the executable is shared by several parts, each of which runs a slice of the functions
        addPartCase(1, "the AST interpreter's output holds the pinned results", \testPinned());
        addPartCase(1, "the AST interpreter's output follows its rules", \testCheck());
        addPartCase(1, "the parts check every execution mode", \testPartsCoverModes());
        addPartCase(1, "the slices of the script give the output of the whole script", \testSlices());
        addPartCase(1, "every execution mode " + Desc + ": ast, ir and tiered-ir", \testModes(),
            (("ast", "ir", ExecModeHarness::IRTierName), 1, 1));
        addPartCase(2, "every execution mode " + Desc + ": jit (1/3)", \testModes(), (("jit",), 1, 3));
        addPartCase(3, "every execution mode " + Desc + ": jit (2/3)", \testModes(), (("jit",), 2, 3));
        addPartCase(4, "every execution mode " + Desc + ": jit (3/3)", \testModes(), (("jit",), 3, 3));
        addPartCase(5, "every execution mode " + Desc + ": tiered (1/3)", \testModes(), (("tiered",), 1, 3));
        addPartCase(6, "every execution mode " + Desc + ": tiered (2/3)", \testModes(), (("tiered",), 2, 3));
        addPartCase(7, "every execution mode " + Desc + ": tiered (3/3)", \testModes(), (("tiered",), 3, 3));
        addPartCase(8, "compiled executable " + Desc + " (1/2)", \testAot(), (1, 2));
        addPartCase(9, "compiled executable " + Desc + " (2/2)", \testAot(), (2, 2));

        set_return_value(main());
    }

    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    testPinned() {
        hash<string, bool> lines = map {$1: True}, CompoundAssignmentMatrixBase::astOutput(1, 1).split("\n");
        list<string> missing = select Pinned, !lines{$1};
        assertEq((), missing);
    }

    testCheck() {
        assertEq((), CompoundAssignmentMatrixBase::resultIsValue(CompoundAssignmentMatrixBase::astOutput(1, 1)));
    }

    #! checks that the modes that the parts run are the execution modes of the harness
    testPartsCoverModes() {
        assertEq(sort(ExecModeHarness::ExecModes), sort(("ast", "ir", "jit", "tiered")),
            "the execution modes that the parts check: update the parts when the harness has another mode");
    }

    #! checks that the AST interpreter's outputs of the slices that the parts run, in order, are the output of the
    #! whole script, so that a function gives the same output in a slice as in the whole script
    testSlices() {
        string whole = CompoundAssignmentMatrixBase::astOutput(1, 1);
        foreach int slices in ((2, 3)) {
            string joined = "";
            for (int slice = 1; slice <= slices; ++slice) {
                joined += CompoundAssignmentMatrixBase::astOutput(slice, slices);
            }
            assertEq((), ExecModeOutputTest::differences(whole, joined), sprintf("%d slices", slices));
        }
    }

    #! runs a slice of the functions in the modes given and checks the output against the AST interpreter's
    /** @param modes the execution modes, or ExecModeHarness::IRTierName for tiered execution on the IR tier
        @param slice the slice of the functions, from 1
        @param slices the number of slices that the functions are divided into
    */
    testModes(list<string> modes, int slice, int slices) {
        TmpDir tmp("qore-compoundassignmentmatrix-");
        string path = writeScript(tmp, slice, slices);
        string expected = CompoundAssignmentMatrixBase::astOutput(slice, slices);
        hash<string, list<string>> wrong();
        foreach string mode in (modes) {
            bool ir_tier = mode == ExecModeHarness::IRTierName;
            string cmd = sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s -b --enable-debug --exec-mode=%s %s",
                libqore_dir, libqore_dir, qore_bin, ir_tier ? "tiered" : mode, path);
            string actual = ir_tier ? ExecModeHarness::run_ir_tier(cmd) : ExecModeHarness::run(cmd);
            list<string> diff = ExecModeOutputTest::differences(expected, actual);
            if (diff) {
                wrong{mode} = diff;
            }
        }
        assertEq({}, wrong);
    }

    #! compiles a slice of the functions with qcc and checks the executable's output against the AST interpreter's
    testAot(int slice, int slices) {
        TmpDir tmp("qore-compoundassignmentmatrix-");
        string path = writeScript(tmp, slice, slices);
        string exe = tmp.path + DirSep + "compoundassignmentmatrix";
        string out = backquote(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s -o %s %s 2>&1",
            libqore_dir, libqore_dir, qcc_bin, exe, path));
        assertTrue(is_file(exe), "compiled the executable: " + out);
        assertEq((), ExecModeOutputTest::differences(CompoundAssignmentMatrixBase::astOutput(slice, slices),
            ExecModeHarness::run_any(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s", libqore_dir,
            libqore_dir, exe))), "executable");
    }

    #! writes the script of a slice of the functions into the directory given and returns its path
    private string writeScript(TmpDir tmp, int slice, int slices) {
        string path = tmp.path + DirSep + "compoundassignmentmatrix.qr";
        File f();
        f.open2(path, O_WRONLY | O_CREAT | O_TRUNC);
        f.write(CompoundAssignmentMatrixBase::script(slice, slices));
        f.close();
        return path;
    }

    #! returns the AST interpreter's output of the script of a slice of the functions, computed once
    static string astOutput(int slice, int slices) {
        string key = sprintf("%d/%d", slice, slices);
        if (!Holder::ast{key}) {
            Holder::ast{key} = ExecModeOutputTest::ast_output("CompoundAssignmentMatrix",
                CompoundAssignmentMatrixBase::script(slice, slices));
        }
        return Holder::ast{key};
    }

    #! returns the lines of the output whose result is not the value the lvalue holds afterwards
    /** A compound assignment gives the value that it stores in the lvalue, as an assignment does (x *= y gives what
        x = x * y gives), whatever the lvalue's type converts; lines with an error have no result
    */
    static list<string> resultIsValue(string output) {
        list<string> rv = ();
        foreach string line in (output.split("\n")) {
            *list<*string> m = (line =~ x/^[^:]*:[^:]*: ([^ ]+) ([^ ]+)$/);
            if (m && m[0] != m[1]) {
                rv += line;
            }
        }
        return rv;
    }

    #! returns the script with a slice of the functions: the declarations, and the functions and their calls
    /** @param slice the slice of the functions, from 1
        @param slices the number of slices that the functions are divided into; 1 for the whole script
    */
    static string script(int slice, int slices) {
        if (!Holder::gen) {
            Holder::gen = CompoundAssignmentMatrixBase::generate();
        }
        int count = Holder::gen.funcs.size();
        int first = count * (slice - 1) / slices;
        int last = count * slice / slices - 1;
        return Holder::gen.prelude + Holder::gen.funcs[first..last].join("\n") + "\n"
            + Holder::gen.calls[first..last].join("\n") + "\n";
    }

    #! returns the declarations of the script
    private static string prelude() {
        list<string> l = ("%modern", "string sub fmt(auto v) { return sprintf(\"%s:%y\", v.type(), v); }",
            "sub out(string label, auto r, auto v) { printf(\"%s: %s %s\\n\", label, fmt(r), fmt(v)); }",
            "sub err(string label, string err) { printf(\"%s: %s\\n\", label, err); }");
        list<string> members = ();
        foreach list<string> t in (Types) {
            string id = CompoundAssignmentMatrixBase::id(t[0]);
            l += sprintf("our %s g_%s;", t[0], id);
            l += sprintf("thread_local %s t_%s;", t[0], id);
            members += sprintf("    %s m_%s;", t[0], id);
            members += sprintf("    static %s s_%s;", t[0], id);
        }
        l += "class C { public {";
        l += members;
        l += "} }";
        return l.join("\n") + "\n";
    }

    #! returns an identifier for a type
    private static string id(string type) {
        return replace(type, "*", "on_");
    }

    #! returns the lvalue expression and its declarations for a kind and a type
    private static list<string> lvalue(string kind, string type, string init) {
        string id = CompoundAssignmentMatrixBase::id(type);
        switch (kind) {
            case "local": return ("v", sprintf("%s v;", type));
            case "global": return ("g_" + id, "");
            case "tl": return ("t_" + id, "");
            case "member": return ("c.m_" + id, "C c();");
            case "static": return ("C::s_" + id, "");
            case "hash": return ("h.k", sprintf("hash<string, %s> h();", type));
            case "list": return ("l[0]", sprintf("list<%s> l = cast<list<%s>>((%s,));", type, type, init));
            case "closure": return ("v", sprintf("%s v; code cl = sub () { v = v; };", type));
            case "ref": return ("r", sprintf("%s v; reference<%s> r = \\v;", type, type));
        }
        throw "MATRIX-ERROR", kind;
    }

    #! generates the script: one function for each lvalue kind and type, holding the combinations that parse
    /** @return a hash: \c prelude: the declarations, \c funcs: the functions, \c calls: their calls, in the same
        order
    */
    private static hash<auto> generate() {
        string prelude = CompoundAssignmentMatrixBase::prelude();
        list<string> funcs = ();
        list<string> calls = ();
        int dropped = 0;
        foreach string kind in (Kinds) {
            foreach list<string> t in (Types) {
                list<string> lv = CompoundAssignmentMatrixBase::lvalue(kind, t[0], t[1]);
                list<string> blocks = ();
                foreach list<string> o in (Operands) {
                    string label = sprintf("%s %s %s:%s", kind, t[0], o[0], replace(o[1], "\"", "'"));
                    string decl = sprintf("    %s o = %s;", o[0], o[1]);
                    list<string> lines = ();
                    foreach string op in (Ops) {
                        # the arguments are evaluated in order: the lvalue is read after the operator changed it
                        string line = sprintf("        try { %s = %s; out(\"%s %s\", %s %s o, %s); } "
                            "catch (hash<ExceptionInfo> ex) { err(\"%s %s\", ex.err); }", lv[0], t[1], label, op,
                            lv[0], op, lv[0], label, op);
                        if (CompoundAssignmentMatrixBase::parses(prelude, lv[1], decl, line)) {
                            lines += line;
                        } else {
                            ++dropped;
                        }
                    }
                    if (lines) {
                        blocks += sprintf("    {\n    %s\n%s\n    }", decl, lines.join("\n"));
                    }
                }
                if (blocks) {
                    string name = sprintf("f_%s_%s", kind, CompoundAssignmentMatrixBase::id(t[0]));
                    funcs += sprintf("sub %s() {\n    %s\n%s\n}", name, lv[1], blocks.join("\n"));
                    calls += name + "();";
                }
            }
        }
        # most combinations of typed operands that do not parse are refused by the parser's type checks, so a
        # matrix with few dropped combinations or none left would show a broken generator
        if (!dropped || !calls) {
            throw "MATRIX-ERROR", sprintf("dropped %d combinations, kept %d functions", dropped, calls.size());
        }
        return {"prelude": prelude, "funcs": funcs, "calls": calls};
    }

    #! returns True if a combination parses without a warning
    private static bool parses(string prelude, string lvdecl, string decl, string line) {
        Program p();
        try {
            *hash<auto> warnings = p.parse(sprintf("%ssub f() {\n    %s\n    {\n    %s\n%s\n    }\n}\n", prelude,
                lvdecl, decl, line), "matrix", WARN_DEFAULT);
            return !warnings;
        } catch (hash<ExceptionInfo> ex) {
            return False;
        }
    }
}

#! holds the generated script and the AST interpreter's output of its slices
class Holder {
    public {
        #! the parts of the script (see CompoundAssignmentMatrixBase::generate())
        static *hash<auto> gen;
        #! the AST interpreter's output by slice ("<slice>/<slices>")
        static hash<string, string> ast;
    }
}
