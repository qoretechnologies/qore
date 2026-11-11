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
