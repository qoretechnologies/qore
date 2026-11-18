%modern
%no-child-restrictions
%requires ./ChildExecModeModule.qm
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

# %push-parse-options restores the options of the child after the parse: they do not support an IR execution mode, so
# the child runs on the AST interpreter in every mode
run_child("push-options", NOTHING, "%push-parse-options\n%modern\nint sub child_push(int x) { return x + 4; }\n",
    "child_push");

# a legacy child creates a Program that its own %modern makes modern: the grandchild inherits the requested mode, which
# the legacy child does not run in itself
{
    Program child(PO_NEW_STYLE);
    child.parse("list sub make_grandchild() {\n"
        "    Program g(PO_NEW_STYLE);\n"
        "    g.parse(\"%modern\\nint sub child_grand(int x) { return x + 6; }\\n\", \"grandchild\");\n"
        "    int rv = 0;\n"
        "    for (int i = 0; i < 3; ++i) {\n"
        "        rv += g.callFunction(\"child_grand\", i);\n"
        "    }\n"
        "    return (g.getExecMode(), rv);\n"
        "}\n", "legacy-parent");
    list<auto> r = child.callFunction("make_grandchild");
    printf("grandchild: mode child=%d grandchild=%d result=%d\n", child.getExecMode(), r[0], r[1]);
}

# a user module runs in a Program of its own, which inherits the requested mode of the Program that loads it
{
    int rv = 0;
    for (int i = 0; i < 3; ++i) {
        rv += module_fn(i);
    }
    printf("module: result=%d\n", rv);
}
