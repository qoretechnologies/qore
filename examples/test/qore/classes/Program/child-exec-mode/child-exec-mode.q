%modern
%no-child-restrictions
# child Programs whose parse options change after they are created; see child-exec-mode.qtest

#! parses the code in a new child Program and calls its function
sub run_child(string label, *code setup, string src, string fn) {
    # the child gets exactly these options (%no-child-restrictions): not those of this Program
    Program p(PO_NEW_STYLE);
    if (setup) {
        setup(p);
    }
    int before = p.getExecMode();
    p.parse(src, label);
    int rv = 0;
    for (int i = 0; i < 3; ++i) {
        rv += p.callFunction(fn, i);
    }
    printf("%s: mode before=%d after=%d result=%d\n", label, before, p.getExecMode(), rv);
}

# the child's own %modern changes its parse options
run_child("modern-directive", NOTHING, "%modern\nint sub child_directive(int x) { return x + 1; }\n",
    "child_directive");

# the parse options are changed with the API
run_child("modern-api", sub (Program p) {
        p.setParseOptions(PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
    }, "int sub child_api(int x) { return x + 2; }\n", "child_api");

# code without %modern cannot run with IR execution: the child runs on the AST interpreter in every mode
run_child("legacy", NOTHING, "int sub child_legacy(int x) { return x + 3; }\n", "child_legacy");
