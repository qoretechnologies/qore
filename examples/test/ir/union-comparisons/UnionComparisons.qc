# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

#! The cases of the UnionComparisons-<n>.qtest tests (issue #5875): each runs one part of them, so that each takes a
#! short time, and together they run every case (see addPartCase())
/** Each case script prints one line per comparison (\c "<label> => <result>"); its output in every execution mode, on
    the IR tier and as AOT executables (qcc -O0 and -O3) must be that of the AST interpreter, whose results are checked
    with the expected results below.

    The AST interpreter specialized == and != for an int when one operand was an int and the other could not be a
    string or a number, so a union<int, float> value of 2.5 was compared as the int 2 (2.5 == 2 was True), and also
    when the other operand had an or-nothing type whose values have a type compared before it (a float, then an int),
    as an or-nothing type is not of its value type for the selection: int == *float compared the values as ints and
    *int == bool as bools.  <, <=, >, >= were specialized for a float when one operand was a float and the other was
    not exactly a number, so a float was compared with a union<int, number> or a *number value as floats, losing the
    number's precision.  The ordering operators now take no float specialization for an operand that can be a number,
    and == and != no int specialization for an operand that can be a float and no bool specialization for one that
    can be a float or an int.

    - union-comparison-cases.q (parts 1 and 2): values of plain, or-nothing number and union types compared with ==,
      !=, <, <=, >, >=, <=> and ===
    - or-nothing-comparison-cases.q (parts 3 and 4): values of or-nothing int, float and bool types and of soft types
      compared with the types of union-comparison-cases.q and with each other with ==, != and <
*/
class UnionComparisonsBase inherits QUnit::Test {
    public {
        #! the union case script, without its extension
        const UnionScript = "union-comparison-cases";
        #! the number of cases that the union case script prints
        const UnionCount = 2592;
        #! results of the AST interpreter checked for the union case script
        const UnionExpected = {
            "union<int, float> == int (2.5, 2)": "False",
            "union<int, float> != int (2.5, 2)": "True",
            "*union<int, float> == int (2.5, 2)": "False",
            # 1.0000000000000000001n, which a float cannot hold
            "float < union<int, number> (1.0, 1)": "True",
            "float < *number (1.0, 1)": "True",
            "*number <= float (1, 1.0)": "False",
            "union<int, number> > float (1, 1.0)": "True",
            "float >= *number (1.0, 1)": "False",
        };

        #! the or-nothing case script, without its extension
        const OrNothingScript = "or-nothing-comparison-cases";
        #! the number of cases that the or-nothing case script prints
        const OrNothingCount = 2295;
        #! results of the AST interpreter checked for the or-nothing case script
        const OrNothingExpected = {
            "int == *float (2, 2.5)": "False",
            "int != *float (2, 2.5)": "True",
            "*float == int (2.5, 2)": "False",
            "int == *softfloat (2, 2.5)": "False",
            "softint == *float (2, 2.5)": "False",
            "*int == bool (2, True)": "False",
            "bool == *int (True, 2)": "False",
            "*float == bool (2.5, True)": "False",
            # the specializations that remain give the same results
            "int == *float (2, 2.0)": "True",
            "*int == int (2, 2)": "True",
            "bool == *bool (True, True)": "True",
            "*float < int (2.5, 2)": "False",
        };
    }

    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;

        string qore_bin;
        string qcc_bin;
        string env;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        qore_bin = ENV.QORE_BIN ?? ENV.QORE_BINARY ?? QORE_ARGV[0] ?? "qore";
        string libqore_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);
        qcc_bin = ENV.QCC ?? ENV.QORE_QCC ?? ENV.QORE_QCC_EXECUTABLE
            ?? (is_file(libqore_dir + "/qcc") ? libqore_dir + "/qcc" : "qcc");
        env = sprintf("LD_LIBRARY_PATH=%s QORE_LIBDIR=%s", libqore_dir, libqore_dir);

        addPartCase(1, "union cases: AST semantics", \astTest(), (UnionScript, UnionCount, UnionExpected));
        addPartCase(1, "union cases: execution modes", \modesTest(), UnionScript);
        addPartCase(2, "union cases: AOT executables", \aotTest(), UnionScript);
        addPartCase(3, "or-nothing cases: AST semantics", \astTest(),
            (OrNothingScript, OrNothingCount, OrNothingExpected));
        addPartCase(3, "or-nothing cases: execution modes", \modesTest(), OrNothingScript);
        addPartCase(4, "or-nothing cases: AOT executables", \aotTest(), OrNothingScript);
        set_return_value(main());
    }

    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! checks the output of the AST interpreter
    private astTest(string script, int count, hash<string, string> expected) {
        list<string> lines = select astOutput(script).split("\n"), $1 =~ / => /;
        assertEq(count, lines.size(), "every case ran");
        hash<string, string> res = map {$1.split(" => ")[0]: $1.split(" => ")[1]}, lines;
        foreach hash<auto> i in (expected.pairIterator()) {
            assertEq(i.value, res{i.key}, i.key);
        }
        assertEq((), (select lines, $1 =~ / => [A-Z][A-Z-]+$/), "no case raised an exception");
    }

    #! runs the script in every execution mode and on the IR tier
    private modesTest(string script) {
        string exp = astOutput(script);
        foreach string mode in (ExecModeHarness::ExecModes) {
            if (mode == "ast") {
                continue;
            }
            assertEq(exp, ExecModeHarness::run(sprintf("%s %s --exec-mode=%s %s", env, qore_bin, mode,
                scriptPath(script))), mode);
        }
        assertEq(exp, ExecModeHarness::run_ir_tier(sprintf("%s %s --exec-mode=tiered %s", env, qore_bin,
            scriptPath(script))), ExecModeHarness::IRTierName);
    }

    #! compiles the script with qcc -O0 and -O3 and checks that the executables give the output of the AST
    #! interpreter
    private aotTest(string script) {
        if (!is_file(qcc_bin)) {
            testSkip("qcc is not available at " + qcc_bin);
        }
        string exp = astOutput(script);
        TmpDir dir(script + "-aot-");
        foreach string opt in ("-O0", "-O3") {
            string exe = sprintf("%s/%s%s", dir.path, script, opt);
            string out = ExecModeHarness::run_any(sprintf("%s %s %s -o %s %s", env, qcc_bin, opt, exe,
                scriptPath(script)));
            assertTrue(is_file(exe), sprintf("%s: qcc compiles the script: %s", opt, out));
            assertEq(exp, ExecModeHarness::run_any(sprintf("%s %s", env, exe)), "AOT " + opt);
        }
    }

    private string scriptPath(string script) {
        return get_script_dir() + "/" + script + ".q";
    }

    #! the output of the script with the AST interpreter
    private string astOutput(string script) {
        return ExecModeHarness::run(sprintf("%s %s --exec-mode=ast %s", env, qore_bin, scriptPath(script)));
    }
}
