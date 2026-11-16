# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The cases of the EnumStringInPlace-<n>.qtest tests: each runs one part of them, so that each takes a short time,
# and together they run every case (see addPartCase())

#! Tests the operations that modify a string in place on string enum lvalues
/** A string enum lvalue holds only the members of its enum, and the value that an operation leaves after modifying
    the lvalue's string in place is a plain string, which the enum type refuses: +=, splice, extract, trim, chomp,
    remove and delete of characters, the regex substitution (=~ s///) and transliteration (=~ tr///) operators raise
    RUNTIME-TYPE-ERROR and leave the lvalue unchanged, in every execution mode.  The AST interpreter asserted that
    the assignment of the string made by a regex substitution or a transliteration could not fail, and native code
    took a reference to the string that the refused assignment had released (qore#6196); the other operations
    replaced the enum value with a plain string, which the lvalue then held (qore#6195).  remove and delete of a
    single character only read it and leave the lvalue unchanged without an error; += on a string enum lvalue with
    no value gave it the enum's first member before appending to it, and now leaves it without a value.  trim and
    chomp of a list or a hash whose element type (or the type of a hashdecl member) is a string enum raise the error
    before they change any element.

    The worker (enum-string-in-place-worker.qr, which make-worker.py makes from the operations and lvalue kinds)
    applies each operation to string enum lvalues with a short member value (held inline) and a long one, of each
    kind: a local, a closure-bound local, a global, an object member (also in a method of its object), a hashdecl
    member, a hash element read with a constant and with a run-time key, a list element, a local changed through a
    reference, a parameter and a local changed through a reference parameter; in a try block, in a function with no
    try block whose caller catches the error, in a try block in a loop, and in top-level code; with and without the
    operation's value used.  Lvalues with no value are left without one, and the operations on a string local and on
    lvalues of no type holding an enum value are controls, which they change.  The members must keep their values.
    The worker is run in each execution mode, on the IR tier, and as a program compiled with qcc at -O0 and -O3;
    every run must give the results of the AST interpreter.
*/
class EnumStringInPlaceBase inherits QUnit::Test {
    public {
        #! the operations, longest name first, and the description in their errors
        const Ops = (
            ("op": "rm-range-rv", "desc": "<remove operator>"),
            ("op": "del-range", "desc": "<delete operator>"),
            ("op": "del-list", "desc": "<delete operator>"),
            ("op": "rm-range", "desc": "<remove operator>"),
            ("op": "rm-list", "desc": "<remove operator>"),
            ("op": "pe-int", "desc": "<+= operator>"),
            ("op": "sp-len", "desc": "<splice operator>"),
            ("op": "sp-rep", "desc": "<splice operator>"),
            ("op": "ex-rep", "desc": "<extract operator>"),
            ("op": "rs-rv", "desc": "<=~ s/// operator>"),
            ("op": "tl-rv", "desc": "<=~ tr/// operator>"),
            ("op": "pe-rv", "desc": "<+= operator>"),
            ("op": "ex-rv", "desc": "<extract operator>"),
            ("op": "tr-rv", "desc": "<trim operator>"),
            ("op": "ch-rv", "desc": "<chomp operator>"),
            ("op": "rm-rv", "desc": "<remove operator>"),
            ("op": "rsg", "desc": "<=~ s/// operator>"),
            ("op": "rs", "desc": "<=~ s/// operator>"),
            ("op": "tl", "desc": "<=~ tr/// operator>"),
            ("op": "pe", "desc": "<+= operator>"),
            ("op": "sp", "desc": "<splice operator>"),
            ("op": "ex", "desc": "<extract operator>"),
            ("op": "tr", "desc": "<trim operator>"),
            ("op": "ch", "desc": "<chomp operator>"),
            ("op": "rm", "desc": "<remove operator>"),
            ("op": "del", "desc": "<delete operator>"),
        );

        #! the operations that only read the lvalue: they raise no error and leave it unchanged
        const ReadOnlyOps = ("rm", "del", "rm-rv");

        #! the character that "rm-rv" removes from the members (an operation that only reads it)
        const RemovedChar = {
            "ess": "string \"a\"",
            "esl": "string \"a\"",
        };

        #! the results of the operations on lvalues with no value, which leave them without one
        const NoValueResults = {
            "pe": "RUNTIME-TYPE-ERROR|<+= operator> expects type '%senum<ESL>', "
                + "but got type 'string' instead|nothing null",
            "sp": "EXTRACT-ERROR|first (lvalue) argument to the extract operator is not a list, string, "
                + "or binary object|nothing null",
            "rm-range": "ILLEGAL-EXPRESSION|Index range can be applied only to lists, strings, binaries, "
                + "and buffers|nothing null",
            "none": "none|nothing null",
        };

        #! the result of each trim and chomp of a container
        const ContainerResults = {
            "cont-hash-tr": "RUNTIME-TYPE-ERROR|<trim operator> expects type 'enum<ESL>', "
                + "but got type 'string' instead|hash<string, "
                + "enum<ESL>> {a: \" a long string value that is not stored inline\\n\", "
                + "b: \" a long string value that is not stored inline\\n\"} [enum<ESL>, enum<ESL>]",
            "cont-hash-ch": "RUNTIME-TYPE-ERROR|<chomp operator> expects type 'enum<ESL>', "
                + "but got type 'string' instead|hash<string, "
                + "enum<ESL>> {a: \" a long string value that is not stored inline\\n\", "
                + "b: \" a long string value that is not stored inline\\n\"} [enum<ESL>, enum<ESL>]",
            "cont-hash-tr-rv": "RUNTIME-TYPE-ERROR|<trim operator> expects type 'enum<ESL>', "
                + "but got type 'string' instead|hash<string, "
                + "enum<ESL>> {a: \" a long string value that is not stored inline\\n\", "
                + "b: \" a long string value that is not stored inline\\n\"} [enum<ESL>, enum<ESL>]|"
                + "nothing null",
            "cont-hash-ch-rv": "RUNTIME-TYPE-ERROR|<chomp operator> expects type 'enum<ESL>', "
                + "but got type 'string' instead|hash<string, "
                + "enum<ESL>> {a: \" a long string value that is not stored inline\\n\", "
                + "b: \" a long string value that is not stored inline\\n\"} [enum<ESL>, enum<ESL>]|"
                + "nothing null",
            "cont-hash-short-tr": "RUNTIME-TYPE-ERROR|<trim operator> expects type 'enum<ESS>', "
                + "but got type 'string' instead|hash<string, enum<ESS>> {a: \" a\\n\"} [enum<ESS>]",
            "cont-hash-short-ch": "RUNTIME-TYPE-ERROR|<chomp operator> expects type 'enum<ESS>', "
                + "but got type 'string' instead|hash<string, enum<ESS>> {a: \" a\\n\"} [enum<ESS>]",
            "cont-hash-short-tr-rv": "RUNTIME-TYPE-ERROR|<trim operator> expects type 'enum<ESS>', "
                + "but got type 'string' instead|hash<string, enum<ESS>> {a: \" a\\n\"} [enum<ESS>]|"
                + "nothing null",
            "cont-hash-short-ch-rv": "RUNTIME-TYPE-ERROR|<chomp operator> expects type 'enum<ESS>', "
                + "but got type 'string' instead|hash<string, enum<ESS>> {a: \" a\\n\"} [enum<ESS>]|"
                + "nothing null",
            "cont-list-tr": "RUNTIME-TYPE-ERROR|<trim operator> expects type 'enum<ESL>', "
                + "but got type 'string' instead|"
                + "list<enum<ESL>> [\" a long string value that is not stored inline\\n\", "
                + "\" a long string value that is not stored inline\\n\"] [enum<ESL>, enum<ESL>]",
            "cont-list-ch": "RUNTIME-TYPE-ERROR|<chomp operator> expects type 'enum<ESL>', "
                + "but got type 'string' instead|"
                + "list<enum<ESL>> [\" a long string value that is not stored inline\\n\", "
                + "\" a long string value that is not stored inline\\n\"] [enum<ESL>, enum<ESL>]",
            "cont-list-tr-rv": "RUNTIME-TYPE-ERROR|<trim operator> expects type 'enum<ESL>', "
                + "but got type 'string' instead|"
                + "list<enum<ESL>> [\" a long string value that is not stored inline\\n\", "
                + "\" a long string value that is not stored inline\\n\"] [enum<ESL>, enum<ESL>]|nothing null",
            "cont-list-ch-rv": "RUNTIME-TYPE-ERROR|<chomp operator> expects type 'enum<ESL>', "
                + "but got type 'string' instead|"
                + "list<enum<ESL>> [\" a long string value that is not stored inline\\n\", "
                + "\" a long string value that is not stored inline\\n\"] [enum<ESL>, enum<ESL>]|nothing null",
            "cont-list-short-tr": "RUNTIME-TYPE-ERROR|<trim operator> expects type 'enum<ESS>', "
                + "but got type 'string' instead|list<enum<ESS>> [\" a\\n\"] [enum<ESS>]",
            "cont-list-short-ch": "RUNTIME-TYPE-ERROR|<chomp operator> expects type 'enum<ESS>', "
                + "but got type 'string' instead|list<enum<ESS>> [\" a\\n\"] [enum<ESS>]",
            "cont-list-short-tr-rv": "RUNTIME-TYPE-ERROR|<trim operator> expects type 'enum<ESS>', "
                + "but got type 'string' instead|list<enum<ESS>> [\" a\\n\"] [enum<ESS>]|nothing null",
            "cont-list-short-ch-rv": "RUNTIME-TYPE-ERROR|<chomp operator> expects type 'enum<ESS>', "
                + "but got type 'string' instead|list<enum<ESS>> [\" a\\n\"] [enum<ESS>]|nothing null",
            "cont-list-or-nothing-tr": "RUNTIME-TYPE-ERROR|<trim operator> expects type '*enum<ESL>', "
                + "but got type 'string' instead|list<*enum<ESL>> [null, "
                + "\" a long string value that is not stored inline\\n\"] [nothing, enum<ESL>]",
            "cont-list-or-nothing-ch": "RUNTIME-TYPE-ERROR|<chomp operator> expects type '*enum<ESL>', "
                + "but got type 'string' instead|list<*enum<ESL>> [null, "
                + "\" a long string value that is not stored inline\\n\"] [nothing, enum<ESL>]",
            "cont-list-or-nothing-tr-rv": "RUNTIME-TYPE-ERROR|<trim operator> expects type '*enum<ESL>', "
                + "but got type 'string' instead|list<*enum<ESL>> [null, "
                + "\" a long string value that is not stored inline\\n\"] [nothing, enum<ESL>]|nothing null",
            "cont-list-or-nothing-ch-rv": "RUNTIME-TYPE-ERROR|<chomp operator> expects type '*enum<ESL>', "
                + "but got type 'string' instead|list<*enum<ESL>> [null, "
                + "\" a long string value that is not stored inline\\n\"] [nothing, enum<ESL>]|nothing null",
            "cont-hashdecl-tr": "RUNTIME-TYPE-ERROR|<trim operator> expects type 'enum<ESL>', "
                + "but got type 'string' instead|hash<MixedHash> {s: \" s \", "
                + "e: \" a long string value that is not stored inline\\n\"} [string, enum<ESL>]",
            "cont-hashdecl-ch": "RUNTIME-TYPE-ERROR|<chomp operator> expects type 'enum<ESL>', "
                + "but got type 'string' instead|hash<MixedHash> {s: \" s \", "
                + "e: \" a long string value that is not stored inline\\n\"} [string, enum<ESL>]",
            "cont-hashdecl-tr-rv": "RUNTIME-TYPE-ERROR|<trim operator> expects type 'enum<ESL>', "
                + "but got type 'string' instead|hash<MixedHash> {s: \" s \", "
                + "e: \" a long string value that is not stored inline\\n\"} [string, enum<ESL>]|nothing null",
            "cont-hashdecl-ch-rv": "RUNTIME-TYPE-ERROR|<chomp operator> expects type 'enum<ESL>', "
                + "but got type 'string' instead|hash<MixedHash> {s: \" s \", "
                + "e: \" a long string value that is not stored inline\\n\"} [string, enum<ESL>]|nothing null",
            "cont-member-hash-tr": "RUNTIME-TYPE-ERROR|<trim operator> expects type 'enum<ESL>', "
                + "but got type 'string' instead|hash<string, "
                + "enum<ESL>> {a: \" a long string value that is not stored inline\\n\"} [enum<ESL>]",
            "cont-member-hash-ch": "RUNTIME-TYPE-ERROR|<chomp operator> expects type 'enum<ESL>', "
                + "but got type 'string' instead|hash<string, "
                + "enum<ESL>> {a: \" a long string value that is not stored inline\\n\"} [enum<ESL>]",
            "cont-member-hash-tr-rv": "RUNTIME-TYPE-ERROR|<trim operator> expects type 'enum<ESL>', "
                + "but got type 'string' instead|hash<string, "
                + "enum<ESL>> {a: \" a long string value that is not stored inline\\n\"} [enum<ESL>]|"
                + "nothing null",
            "cont-member-hash-ch-rv": "RUNTIME-TYPE-ERROR|<chomp operator> expects type 'enum<ESL>', "
                + "but got type 'string' instead|hash<string, "
                + "enum<ESL>> {a: \" a long string value that is not stored inline\\n\"} [enum<ESL>]|"
                + "nothing null",
            "cont-auto-hash-tr": "none|"
                + "hash<auto> {a: \"a long string value that is not stored inline\", s: \"s\"} [string, "
                + "string]",
            "cont-auto-hash-ch": "none|hash<auto> {a: \" a long string value that is not stored inline\", "
                + "s: \" s \"} [string, string]",
            "cont-auto-hash-tr-rv": "none|"
                + "hash<auto> {a: \"a long string value that is not stored inline\", s: \"s\"} [string, "
                + "string]|hash<auto> {a: \"a long string value that is not stored inline\", s: \"s\"}",
            "cont-auto-hash-ch-rv": "none|hash<auto> {a: \" a long string value that is not stored inline\", "
                + "s: \" s \"} [string, string]|integer 1",
            "cont-auto-list-tr": "none|list<auto> [\"a long string value that is not stored inline\", \"s\"] [string, "
                + "string]",
            "cont-auto-list-ch": "none|"
                + "list<auto> [\" a long string value that is not stored inline\", \" s \"] [string, "
                + "string]",
            "cont-auto-list-tr-rv": "none|"
                + "list<auto> [\"a long string value that is not stored inline\", \"s\"] [string, "
                + "string]|list<auto> [\"a long string value that is not stored inline\", \"s\"]",
            "cont-auto-list-ch-rv": "none|"
                + "list<auto> [\" a long string value that is not stored inline\", \" s \"] [string, "
                + "string]|integer 1",
        };

        #! the members of the enums of the lvalues as the worker prints them
        const Members = {
            "ess": "enum<ESS> \" a\\n\"",
            "esl": "enum<ESL> \" a long string value that is not stored inline\\n\"",
        };

        #! the values of the members of the enums, which no operation changes
        const MemberValues = "\" a\\n\" \" a long string value that is not stored inline\\n\"";

        #! the groups of control checks, whose lvalues the operations change
        const Controls = ("string-in", "auto-in", "auto-short-in", "auto-hash-in");

        #! the execution modes that each part runs: together every mode of ExecModeHarness::ExecModes
        const PartModes = (("ast", "ir", "jit"), ("tiered",));

        #! the number of checks the worker makes
        const CheckCount = 963;
    }

    private {
        string qore_bin;
        string libqore_dir;
        *string qcc_bin;
        #! the description of each operation
        hash<string, string> op_desc;
        #! matches the operation that ends the name of a check
        string op_regex;
        #! the results of the AST interpreter, which every other run must give
        hash<string, string> ast_results;
        #! the part of the cases that this test runs (see addPartCase())
        int part;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        # the interpreter and qcc come from TestInterpreter.qm: a compiled test must not run itself
        qore_bin = TestInterpreter::qore_interpreter();
        libqore_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);
        qcc_bin = TestInterpreter::qcc_compiler();
        op_desc = map {$1.op: $1.desc}, Ops;
        op_regex = "^(.+)-(" + (map $1.op, Ops).join("|") + ")$";

        # the jit and tiered runs compile every function natively and take the most time
        addPartCase(1, "execution modes ast, ir and jit", \testExecModes(), (PartModes[0],));
        addPartCase(2, "execution mode tiered", \testExecModes(), (PartModes[1],));
        addPartCase(2, "IR tier", \testIrTier());
        addPartCase(2, "compiled program", \testCompiledProgram());

        set_return_value(main());
    }

    #! adds a test case if it is in the part that this test runs
    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! runs the worker in the given execution modes; the parts together run it in every mode of
    #! ExecModeHarness::ExecModes
    testExecModes(list<string> modes) {
        assertEq(sort(ExecModeHarness::ExecModes), sort(PartModes[0] + PartModes[1]), "the parts run every mode");
        foreach string mode in (modes) {
            checkWorkerOutput(mode, ExecModeHarness::run(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s -b "
                "--enable-debug --exec-mode=%s %s", libqore_dir, libqore_dir, qore_bin, mode, getWorker())));
        }
    }

    #! every function runs on the IR tier, which the tiered run of ExecModes hardly runs
    testIrTier() {
        checkWorkerOutput("ir-tier", ExecModeHarness::run_ir_tier(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s "
            "%s -b --enable-debug --exec-mode=tiered %s", libqore_dir, libqore_dir, qore_bin, getWorker())));
    }

    testCompiledProgram() {
        if (!qcc_bin || !is_file(qcc_bin)) {
            testSkip(sprintf("no qcc binary found (%y)", qcc_bin));
        }
        TmpDir tmp("qore-enum-string-in-place-");
        foreach int level in (0, 3) {
            string exe = sprintf("%s%senum-string-in-place-O%d", tmp.path, DirSep, level);
            string compile_out = backquote(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s -O%d --strip-source "
                "-o %s %s 2>&1", libqore_dir, libqore_dir, qcc_bin, level, exe, getWorker()));
            assertTrue(is_file(exe), sprintf("compiled program -O%d: %s", level, compile_out));
            checkWorkerOutput(sprintf("aot -O%d", level), ExecModeHarness::run_any(sprintf("LD_LIBRARY_PATH=%s "
                "DYLD_LIBRARY_PATH=%s %s", libqore_dir, libqore_dir, exe)));
        }
    }

    private string getWorker() {
        return get_script_dir() + DirSep + "enum-string-in-place-worker.qr";
    }

    #! returns the results of the AST interpreter, which are checked against the expected semantics
    private hash<string, string> getAstResults() {
        if (!ast_results) {
            hash<string, string> results = parseWorkerOutput("ast", ExecModeHarness::run(sprintf(
                "LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s -b --enable-debug --exec-mode=ast %s", libqore_dir,
                libqore_dir, qore_bin, getWorker())));
            assertEq(CheckCount, results.size(), "ast: checks run");
            foreach hash<auto> i in (results.pairIterator()) {
                checkAstResult(i.key, i.value);
            }
            ast_results = results;
        }
        return ast_results;
    }

    #! returns the error that an operation on an lvalue of the enum with the given prefix raises
    private string getError(string op, string prefix) {
        return sprintf("RUNTIME-TYPE-ERROR|%s expects type 'enum<%s>', but got type 'string' instead",
            op_desc{op}, prefix.upr());
    }

    #! checks a result of the AST interpreter: every operation on a string enum lvalue that would change it raises the
    #! error and leaves it unchanged, an lvalue with no value is left without one, and the operations change the
    #! control lvalues
    private checkAstResult(string name, string result) {
        if (name == "es-members") {
            assertEq(MemberValues, result, "ast: " + name);
            return;
        }
        if (name =~ /^cont-/) {
            assertEq(ContainerResults{name}, result, "ast: " + name);
            return;
        }
        *list<*string> parts = regex_extract(name, op_regex);
        assertTrue(exists parts, "ast: operation of " + name);
        string group = parts[0];
        string op = parts[1];
        string rv_tail = op =~ /-rv$/ ? "|nothing null" : "";
        if (inlist(group, Controls)) {
            # an operation that only reads the lvalue leaves the enum value of an untyped lvalue
            assertRegex(inlist(op, ReadOnlyOps) ? "^none\\|" : "^none\\|string \"", result, "ast: " + name);
            if (rv_tail && op != "rm-rv" && op != "rm-range-rv" && op != "ex-rv" && op != "ch-rv") {
                # the value of the operation is the new value of the lvalue
                list<string> l = result.split("|");
                assertEq(l[1], l[2], "ast: value of " + name);
            }
            return;
        }
        if (group =~ /^eslnv-/) {
            assertEq(getNoValueResult(group, op), result, "ast: " + name);
            return;
        }
        *list<*string> eparts = (group =~ x/^(ess|esl)-(.+)$/);
        assertTrue(exists eparts, "ast: kind of " + name);
        string prefix = eparts[0];
        string kind = eparts[1];
        string member = Members{prefix};
        if (inlist(op, ReadOnlyOps)) {
            string tail = op == "rm-rv" ? "|" + RemovedChar{prefix} : "";
            if (kind == "local-out") {
                assertEq("none", result, "ast: " + name);
            } else if (kind == "local-loop") {
                assertEq(sprintf("none|%s (caught 0)", member), result, "ast: " + name);
            } else {
                assertEq(sprintf("none|%s%s", member, tail), result, "ast: " + name);
            }
            return;
        }
        string error = getError(op, prefix);
        if (kind == "local-out") {
            assertEq(error, result, "ast: " + name);
        } else if (kind == "local-loop") {
            assertEq(sprintf("none|%s (caught 50)", member), result, "ast: " + name);
        } else {
            assertEq(sprintf("%s|%s%s", error, member, rv_tail), result, "ast: " + name);
        }
    }

    #! returns the result of an operation on an ESL lvalue with no value
    private string getNoValueResult(string group, string op) {
        string rv_tail = op =~ /-rv$/ ? "|nothing null" : "";
        string base = op =~ /-rv$/ ? op.substr(0, -3) : op;
        if (base == "pe" || base == "pe-int") {
            # the type of an or-nothing lvalue: "*ESL" locals and members, and the hash<string, *ESL> element
            string ornothing = group =~ /-or-nothing-|^eslnv-hash-/ ? "*" : "";
            return sprintf(NoValueResults.pe, ornothing) + rv_tail;
        }
        if (base =~ /^(sp|ex)/) {
            return NoValueResults.sp + rv_tail;
        }
        if (base == "rm-range" || base == "del-range") {
            return NoValueResults."rm-range" + rv_tail;
        }
        return NoValueResults.none + rv_tail;
    }

    #! parses the "name=result" lines printed by the worker; any other output fails
    private hash<string, string> parseWorkerOutput(string label, string out) {
        hash<string, string> results = {};
        foreach string line in (out.split("\n")) {
            *list<*string> parts = (line =~ x/^([-a-z]+)=(.*)$/);
            if (!parts) {
                if (line.val()) {
                    fail(sprintf("%s: unexpected worker output: %y", label, out));
                }
                continue;
            }
            assertFalse(results.hasKey(parts[0]), sprintf("%s: %s printed once", label, parts[0]));
            results{parts[0]} = parts[1];
        }
        return results;
    }

    #! checks the results of a run against those of the AST interpreter
    private checkWorkerOutput(string label, string out) {
        hash<string, string> expected = getAstResults();
        hash<string, string> results = parseWorkerOutput(label, out);
        assertEq(sort(keys expected), sort(keys results), label + ": checks run");
        foreach hash<auto> i in (expected.pairIterator()) {
            assertEq(i.value, results{i.key}, sprintf("%s: %s", label, i.key));
        }
    }
}
