# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# the driver of the restricted script that module-public-var-readonly.qtest compiles with qcc: the test prepends the
# parse directives, Forms.ql and the functions of the restricted code (qore#5664)

sub result(string key, string value) {
    printf("%s=%s\n", key, value);
}

sub host_write(reference r) {
    r = 12;
}

OurVarMod::reset();
foreach string form in (keys ModVarForms::WriteForms) {
    string r = "ok";
    try {
        call_function(ModVarForms::fname("w_", form));
    } catch (hash<ExceptionInfo> ex) {
        r = ex.err;
    }
    result("aot." + form, r);
}
result("aot.state", OurVarMod::state());
foreach string form in (keys ModVarForms::ReadForms) {
    try {
        result("aot." + form, sprintf("%y", call_function(ModVarForms::fname("r_", form))));
    } catch (hash<ExceptionInfo> ex) {
        result("aot." + form, ex.err);
    }
}
result("aot.state-after-reads", OurVarMod::state());
foreach string form in (("module-setter", "static-setter")) {
    OurVarMod::reset();
    string r = "ok";
    try {
        call_function(ModVarForms::fname("c_", form));
    } catch (hash<ExceptionInfo> ex) {
        r = ex.err;
    }
    result("aot." + form, r);
    result("aot." + form + "-state", sprintf("%d:%d", OurVarMod::i, OurVarMod::Obj::sx));
}

# an independent Program that the restricted script creates keeps its restrictions (qore#5669): it cannot change the
# module's state, and it and an independent Program that it creates cannot parse code of the PROCESS domain
OurVarMod::reset();
{
    Program c(PO_NEW_STYLE, True);
    string r = "ok";
    try {
        c.parse("%requires OurVarMod\nsub w() { OurVarMod::i = 7; }\n", "independent");
        c.callFunction("w");
    } catch (hash<ExceptionInfo> ex) {
        r = ex.err;
    }
    result("aot.independent-child", r);
}
result("aot.independent-child-state", OurVarMod::state());
const ProcessCode = "int sub t() { return umask(022); }\n";
# the error of a parse that the restriction refuses: it names the function, or the error is given with its message
string sub process_err(hash<ExceptionInfo> ex) {
    return ex.desc =~ /umask/ ? ex.err : sprintf("%s: %s", ex.err, ex.desc);
}
{
    Program c(PO_NEW_STYLE, True);
    string r = "ok";
    try {
        c.parse(ProcessCode, "independent-process");
    } catch (hash<ExceptionInfo> ex) {
        r = process_err(ex);
    }
    result("aot.independent-child-process", r);
}
{
    Program c(PO_NEW_STYLE, True);
    c.parse("string sub g(string code) {\n"
        "    Program d(PO_NEW_STYLE, True);\n"
        "    try {\n"
        "        d.parse(code, \"nested-independent-process\");\n"
        "    } catch (hash<ExceptionInfo> ex) {\n"
        "        return ex.desc =~ /umask/ ? ex.err : sprintf(\"%s: %s\", ex.err, ex.desc);\n"
        "    }\n"
        "    return \"ok\";\n"
        "}\n", "independent-creator");
    result("aot.nested-independent-child-process", c.callFunction("g", ProcessCode));
}
