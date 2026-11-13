# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

#! The cases of the ArithParseResultTypes-<n>.qtest tests (issues #5663 and #5876): each runs one part of them, so that
#! each takes a short time, and together they run every case (see addPartCase())
/** The parse result type of an arithmetic operator was taken from the type of one operand when it had one value type,
    whatever values the other operand could have (ex: int for <tt>*float + int</tt>, float for <tt>number * float</tt>),
    so the AST interpreter raised RUNTIME-TYPE-ERROR for a typed container built from the result, and the compiled
    modes, which trust the type, did too.  The result type is now the union of the runtime results of every combination
    of the values that the operand types allow (QoreArithmeticParseType.h).

    Each case script prints one line per case (\c "<label> => <result>"); its output in every execution mode, on the
    IR tier and as AOT executables (qcc -O0 and -O3) must be that of the AST interpreter, whose results are checked with
    the expected results below.

    - arith-result-cases.q (parts 1-4: IR and JIT, tiered and the IR tier, AOT -O0, AOT -O3): +, -, * and / applied
      to parameters of plain, or-nothing, union and container types, with the result in a list literal
    - arith-list-cases.q (part 5): list + list, list + element and element + list for list and element types (plain,
      soft, or-nothing, typed and untyped), with the result in a list literal in a named function
    - arith-container-cases.q (part 5): map expressions over typed lists, whose element type is the parse result type
      of the operator, with containers, unions, or-nothing, soft, untyped, timeout, date, bool, binary and char
      operands

    A list that + gives has a parse-time type only when it does not depend on the types of the values at runtime (a
    list of int, float, number, string, bool or binary and a value of one of these types); otherwise it has none, and
    every mode gives the result of the AST interpreter (an error in its use, ex: assigning list<softint> + int to an
    int, is raised at runtime; see the list section of the + operator in the language documentation).
*/
class ArithParseResultTypesBase inherits QUnit::Test {
    public {
        #! the matrix case script, without its extension
        const MatrixScript = "arith-result-cases";
        #! the number of cases that the matrix case script prints
        const MatrixCount = 2304;
        #! results of the AST interpreter checked for the matrix case script
        const MatrixExpected = {
            "number * float (2.5, 2.5)": "[6.25]",
            "date * float (<time: 1 hour>, 2.5)": "[<time: 2 hours 30 minutes>]",
            "*number * float (2.5, 2.5)": "[6.25]",
            "*float + int (2.5, 2)": "[4.5]",
            "*float + int (null, 2)": "[2]",
            "union<int, float> + int (2.5, 2)": "[4.5]",
            "union<int, string> + int (\"3\", 2)": "[\"32\"]",
            "*union<int, number> - int (2.5, 2)": "[0.5]",
            "*union<int, number> - int (null, 2)": "[-2]",
            # container results: the rules of an operator for operands of one type each that give another value are
            # not used (bool + hash was typed nothing, object + object hash)
            "bool + hash<auto> (True, {a: 1})": "[{a: 1}]",
            "binary + hash<auto> (!!binary AQ==, {a: 1})": "[{a: 1}]",
            "C + C (object<C>, object<C>)": "[\"object<C>\"]",
            "C - *int (object<C>, null)": "[\"object<C>\"]",
            "hash<auto> - string ({a: 1}, \"3\")": "[{a: 1}]",
            # division: an int result where both operands are ints at runtime, a number where either is a number
            "*int / int (2, 2)": "[1]",
            "union<int, float> / int (2, 2)": "[1]",
            "*number / float (2.5, 2.5)": "[1]",
            "float / *union<int, number> (2.5, 2.5)": "[1]",
        };

        #! the container case script, without its extension
        const ContainerScript = "arith-container-cases";
        #! the number of cases that the container case script prints
        const ContainerCount = 66;
        #! results of the AST interpreter checked for the container case script: the list, its full type and the full
        #! type of its first element
        const ContainerExpected = {
            # the container types that the rules of the operators keep
            "list<int> + int": "[[1, 2, 4], [3, 4]] list<list<int>> list<int>",
            "list<int> + list<int>": "[[1, 2, 5, 6], [3, 5, 6]] list<list<int>> list<int>",
            "list<int> + list<string>": "[[1, 2, \"a\"], [3, \"a\"]] list<list<auto>> list<auto>",
            # a soft list type of an operand is not the type of the result, which is the type of the list that the
            # runtime gives (the common type of the element types)
            "softlist<int> + softlist<int>": "[[1, 1]] list<list<int>> list<int>",
            "softlist<int> + int": "[[1, 2]] list<list<int>> list<int>",
            "list<int> + list<int> (literal)": "[[1, 2]] list<list<int>> list<int>",
            "list<int> + list<string> (literal)": "[[1, \"a\"]] list<list<auto>> list<auto>",
            "list<*int> + int (literal)": "[[1, 2]] list<list<*int>> list<*int>",
            "int + list<int> (literal)": "[[2, 1]] list<list<int>> list<int>",
            "int + list<int>": "[[4, 1, 2], [4, 3]] list<list<int>> list<int>",
            # an int or string member initialized with an enum value keeps the enum type, which the runtime takes
            "int member (enum value) + list<Pr>": "[[10, 0]] list<list<enum<Pr>>> list<enum<Pr>>",
            "string member (enum value) + list<St>": "[[\"a\", \"a\"]] list<list<enum<St>>> list<enum<St>>",
            "int member (enum value) + list<int>": "[[10, 1]] list<list<int>> list<int>",
            # a result type that depends on the values is not known at parse time: an error at runtime
            "list<softint> + int assigned to int": "RUNTIME-TYPE-ERROR",
            "softlist<int> + softlist<int> (element assignment)": "RUNTIME-TYPE-ERROR",
            "softlist<int> + int (element assignment)": "RUNTIME-TYPE-ERROR",
            "hash<D> + hash<D>": "[{x: 3, s: \"a\"}, {x: 3, s: \"a\"}] list<hash<D>> hash<D>",
            "hash<D> - string": "[{s: \"a\"}, {s: \"a\"}] list<hash<D>> hash<D>",
            "hash<string, int> + hash<string, int>":
                "[{a: 1, c: 3}, {b: 2, c: 3}] list<hash<string, int>> hash<string, int>",
            "*hash<auto> + hash<D>": "[{a: 1, x: 3, s: \"a\"}, {x: 3, s: \"a\"}] list<hash<auto>> hash<auto>",
            # qore#5876: the AST interpreter applies the result type of a map expression
            "union<int, float> + int": "[2, 3.5] list<union<int, float>> integer",
            "union<int, number> * float": "[5.0, 6.25] list<union<float, number>> float",
            "*float + int": "[4.5, 2] list<union<int, float>> float",
            "*int / int": "[1, 0.0] list<union<int, float>> integer",
            "float * *number": "[5, 0.0] list<union<float, number>> number",
            "softint + float": "[1.5, 2.5] list<float> float",
            "softnumber - float": "[1, 1.5] list<number> number",
            "auto + int": "[2, 3.5, 4, \"41\", <time: 3 seconds>, 2, 1] list<auto> integer",
            "timeout + date": "[2026-01-01 00:00:01.000000 +00:00, 2026-01-01 00:00:02.500000 +00:00] list<date> date",
            "char + int": "[98, 99] list<int> integer",
            "char / int": "[48, 49] list<int> integer",
        };

        #! the list case script, without its extension
        const ListScript = "arith-list-cases";
        #! the number of cases that the list case script prints
        const ListCount = 1037;
        #! results of the AST interpreter checked for the list case script: the list literal, its full type and the
        #! full type of the result
        const ListExpected = {
            "list<int> + list<*int> ((1,), (1,))": "[[1, 1]] list<list<*int>> list<*int>",
            "softlist<int> + list<int> ((1,), (1,))": "[[1, 1]] list<list<int>> list<int>",
            "list + list ((1,), (1,))": "[[1, 1]] list<list> list",
            "list<auto> + list<int> ((), (1,))": "[[1]] list<list<int>> list<int>",
            "list + int ((1,), 2)": "[[1, 2]] list<list<auto>> list<auto>",
            # element + list keeps the type of the list as list + element does (qore#5936)
            "int + list<int> (2, (1,))": "[[2, 1]] list<list<int>> list<int>",
            "int + list<*int> (2, (1,))": "[[2, 1]] list<list<*int>> list<*int>",
            "int + list<auto> (2, (1, \"a\"))": "[[2, 1, \"a\"]] list<list<auto>> list<auto>",
            "int + list<auto> (2, ())": "[[2]] list<list<int>> list<int>",
            "string + list<int> (\"s\", (1,))": "[[\"s\", 1]] list<list<auto>> list<auto>",
            "list<*int> + int ((1,), 2)": "[[1, 2]] list<list<*int>> list<*int>",
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

        addPartCase(1, "matrix cases: AST semantics", \astTest(), (MatrixScript, MatrixCount, MatrixExpected));
        addPartCase(1, "matrix cases: IR and JIT", \modesTest(), (MatrixScript, ("ir", "jit"), False));
        addPartCase(2, "matrix cases: tiered and IR tier", \modesTest(), (MatrixScript, ("tiered",), True));
        addPartCase(3, "matrix cases: AOT -O0", \aotTest(), (MatrixScript, True, ("-O0",)));
        addPartCase(4, "matrix cases: AOT -O3", \aotTest(), (MatrixScript, True, ("-O3",)));
        addPartCase(5, "container cases: AST semantics", \astTest(),
            (ContainerScript, ContainerCount, ContainerExpected));
        addPartCase(5, "container cases: execution modes", \modesTest(), ContainerScript);
        addPartCase(5, "container cases: AOT executables", \aotTest(), (ContainerScript, False));
        addPartCase(5, "list cases: AST semantics", \astTest(), (ListScript, ListCount, ListExpected));
        addPartCase(5, "list cases: execution modes", \modesTest(), ListScript);
        addPartCase(5, "list cases: AOT executables", \aotTest(), (ListScript, False));
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
        # no result type refuses the value of the operator (an element assignment case assigns a value that the
        # result type refuses, and an "assigned to" case assigns the result to a variable of another type)
        assertEq((), (select lines, $1 =~ /RUNTIME-TYPE-ERROR/
            && $1 !~ /(\(element assignment\)| assigned to [a-z]+) => /), "result types allow every result");
        hash<string, string> res = map {$1.split(" => ")[0]: $1.split(" => ")[1]}, lines;
        foreach hash<auto> i in (expected.pairIterator()) {
            assertEq(i.value, res{i.key}, i.key);
        }
    }

    #! runs the script in the given execution modes (every mode but ast by default) and on the IR tier
    private modesTest(string script, *list<string> modes, bool ir_tier = True) {
        string exp = astOutput(script);
        foreach string mode in (modes ?? (select ExecModeHarness::ExecModes, $1 != "ast")) {
            assertEq(exp, ExecModeHarness::run(sprintf("%s %s --exec-mode=%s %s", env, qore_bin, mode,
                scriptPath(script))), mode);
        }
        if (ir_tier) {
            assertEq(exp, ExecModeHarness::run_ir_tier(sprintf("%s %s --exec-mode=tiered %s", env, qore_bin,
                scriptPath(script))), ExecModeHarness::IRTierName);
        }
    }

    #! compiles the script with qcc with the given optimization levels (-O0 and -O3 by default) and checks that the
    #! executables give the output of the AST interpreter
    /** With \a skip_or_nothing, the cases with an operand of an or-nothing type are left out: AOT code takes NOTHING
        as 0 in arithmetic on such operands of the matrix script (qore#5889)
    */
    private aotTest(string script, bool skip_or_nothing = True, list<string> opts = ("-O0", "-O3")) {
        if (!is_file(qcc_bin)) {
            testSkip("qcc is not available at " + qcc_bin);
        }
        list<string> exp = select astOutput(script).split("\n"), !skip_or_nothing || $1 !~ /(^|\s)\*[a-z]/;
        TmpDir dir(script + "-aot-");
        foreach string opt in (opts) {
            string exe = sprintf("%s/%s%s", dir.path, script, opt);
            string out = ExecModeHarness::run_any(sprintf("%s %s %s -o %s %s", env, qcc_bin, opt, exe,
                scriptPath(script)));
            assertTrue(is_file(exe), sprintf("%s: qcc compiles the script: %s", opt, out));
            list<string> actual = select ExecModeHarness::run_any(sprintf("%s %s", env, exe)).split("\n"),
                !skip_or_nothing || $1 !~ /(^|\s)\*[a-z]/;
            assertEq(exp, actual, "AOT " + opt);
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
