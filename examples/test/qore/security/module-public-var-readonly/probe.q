#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# runs every form of Forms.ql in restricted and trusted Programs and prints the results (qore#5664)

%modern

%requires OurVarMod
%requires LibMod

%include Forms.ql

sub result(string key, string value) {
    printf("%s=%s\n", key, value);
}

sub host_write(reference r) {
    r = 12;
}

sub host_set_i() {
    OurVarMod::i = 13;
}

code sub get_host_closure() {
    return sub () { OurVarMod::i = 13; };
}

#! returns "ok" or the exception code of calling the function
string sub try_call(Program p, string func) {
    try {
        p.callFunction(func);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return "ok";
}

#! returns the value of calling the function as "%y", or its exception code
string sub try_read(Program p, string func) {
    try {
        return sprintf("%y", p.callFunction(func));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub called_state() {
    return sprintf("%d:%d", OurVarMod::i, OurVarMod::Obj::sx);
}

const WriterSource = ModVarForms::get_writer_source();

Program sub make_program(string ctx) {
    int po = PO_NEW_STYLE;
    if (ctx == "no-pc") {
        po |= PO_NO_PROCESS_CONTROL;
    } else if (ctx == "lockdown") {
        po |= PO_LOCKDOWN;
    }
    Program p(po);
    if (ctx == "sandbox") {
        p.setSandboxManager(new SandboxManager());
    }
    map p.importFunction($1), ("host_write", "host_set_i", "get_host_closure");
    if (ctx == "lockdown") {
        # the Program cannot load modules: the host gives it the module's variables, classes and functions
        map p.importGlobalVariable("OurVarMod::" + $1), ("i", "l", "h", "s", "o", "g", "hn");
        # the classes of the imported variables are imported with them
        foreach string cls in (("Obj", "G")) {
            try {
                p.importClass("OurVarMod::" + cls);
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err != "CLASS-IMPORT-ERROR") {
                    rethrow;
                }
            }
        }
        map p.importFunction($1), ("OurVarMod::set_i", "OurVarMod::push_l", "OurVarMod::set_sx_func",
            "LibMod::set_other", "PathMod::set_other");
        p.parse(WriterSource, "writer");
    } else {
        p.parse("%requires OurVarMod\n%requires LibMod\n%requires PathMod\n" + WriterSource, "writer");
    }
    if (ctx == "sandbox-after") {
        p.setSandboxManager(new SandboxManager());
    } else if (ctx == "no-pc-after") {
        p.setParseOptions(PO_NO_PROCESS_CONTROL);
    }
    return p;
}

# a module that the trusted host loads by path: trusted, but not library code
load_module(get_script_dir() + "bypath/PathMod.qm");

foreach string ctx in (("sandbox", "sandbox-after", "no-pc", "no-pc-after", "lockdown")) {
    OurVarMod::reset();
    Program p = make_program(ctx);
    foreach string form in (keys ModVarForms::WriteForms) {
        result(ctx + "." + form, try_call(p, ModVarForms::fname("w_", form)));
    }
    # a closure that the restricted code created, called by the trusted host
    try {
        p.callFunction("get_closure")();
        result(ctx + ".closure-host", "ok");
    } catch (hash<ExceptionInfo> ex) {
        result(ctx + ".closure-host", ex.err);
    }
    # the trusted host sets the restricted Program's import
    try {
        p.setGlobalVarValue("OurVarMod::i", 5);
        result(ctx + ".set-global-var-value", "ok");
    } catch (hash<ExceptionInfo> ex) {
        result(ctx + ".set-global-var-value", ex.err);
    }
    result(ctx + ".state", OurVarMod::state());
    foreach string form in (keys ModVarForms::ReadForms) {
        result(ctx + "." + form, try_read(p, ModVarForms::fname("r_", form)));
    }
    # reading through references changed nothing
    result(ctx + ".state-after-reads", OurVarMod::state());
    foreach string form in (keys ModVarForms::CalledForms) {
        OurVarMod::reset();
        result(ctx + "." + form, try_call(p, ModVarForms::fname("c_", form)));
        result(ctx + "." + form + "-state", called_state());
    }
}

# a trusted importer: every form of write succeeds, and every form of read gives the same value
foreach string form in (keys ModVarForms::WriteForms) {
    OurVarMod::reset();
    Program p = make_program("trusted");
    result("trusted." + form, try_call(p, ModVarForms::fname("w_", form)));
}
foreach string form in (keys ModVarForms::ReadForms) {
    OurVarMod::reset();
    Program p = make_program("trusted");
    result("trusted." + form, try_read(p, ModVarForms::fname("r_", form)));
}

# reflection
foreach string ctx in (("sandbox", "no-pc")) {
    OurVarMod::reset();
    Program p(PO_NEW_STYLE | (ctx == "no-pc" ? PO_NO_PROCESS_CONTROL : 0));
    if (ctx == "sandbox") {
        p.setSandboxManager(new SandboxManager());
    }
    p.parse("%requires OurVarMod\n%requires reflection\n"
        "StaticMember sub get_static() { Class c = Class::forName(\"OurVarMod::Obj\"); return c.getStaticMember(\"sx\"); }\n"
        "GlobalVar sub get_global() { return GlobalVar::forName(\"OurVarMod::i\"); }\n"
        "sub static_set() { StaticMember m = get_static(); m.setValue(85); }\n"
        "auto sub static_get() { StaticMember m = get_static(); return m.getValue(); }\n"
        "sub global_set() { GlobalVar v = get_global(); v.setValue(86); }\n"
        "auto sub global_get() { GlobalVar v = get_global(); return v.getValue(); }\n", "reflection");
    result("reflection-" + ctx + ".static-set", try_call(p, "static_set"));
    result("reflection-" + ctx + ".static-get", try_read(p, "static_get"));
    result("reflection-" + ctx + ".global-set", try_call(p, "global_set"));
    result("reflection-" + ctx + ".global-get", try_read(p, "global_get"));
    result("reflection-" + ctx + ".state", OurVarMod::state());
}

# the messages name what is written and the module, and say what to do instead
sub check_message(string key, Program p, string func, string what) {
    try {
        p.callFunction(func);
        result(key, "no exception");
    } catch (hash<ExceptionInfo> ex) {
        bool ok = ex.err == ModVarForms::ErrCode && regex(ex.desc, what)
            && (ex.desc =~ /the module must export a public function or method that sets it/);
        result(key, ok ? "ok" : ex.desc);
    }
}
{
    OurVarMod::reset();
    Program p = make_program("sandbox");
    check_message("message", p, "w_assign", "^cannot change public variable 'OurVarMod::i' of module 'OurVarMod': "
        "sandboxed code, and code in a Program with PO_NO_PROCESS_CONTROL set, can read");
    check_message("message-static", p, "w_static_assign", "^cannot change static member 'OurVarMod::Obj::sx' of "
        "module 'OurVarMod': sandboxed code");
}

# the host's own writes
{
    OurVarMod::reset();
    OurVarMod::i = 5;
    push OurVarMod::l, 4;
    OurVarMod::Obj::sx = 6;
    result("host", OurVarMod::i == 5 && OurVarMod::l == (1, 2, 3, 4) && OurVarMod::Obj::sx == 6
        ? "ok" : OurVarMod::state());
}

# a Program that restricted code creates is restricted as well: a child Program inherits the parent's parse options
# (qore#5664); a child of a sandboxed Program is sandboxed with qore#5659
foreach string ctx in (("sandbox", "no-pc")) {
    OurVarMod::reset();
    Program s(PO_NEW_STYLE | (ctx == "no-pc" ? PO_NO_PROCESS_CONTROL : 0));
    if (ctx == "sandbox") {
        s.setSandboxManager(new SandboxManager());
    }
    s.parse("sub w(bool s) {\n"
        "    Program c(PO_NEW_STYLE);\n"
        "    c.parse(\"%requires OurVarMod\\n\"\n"
        "        \"sub w(bool s) { if (s) { OurVarMod::Obj::sx = 7; } else { OurVarMod::i = 7; } }\", \"child\");\n"
        "    c.callFunction(\"w\", s);\n"
        "}\n", "creator");
    try {
        s.callFunction("w", False);
        result(ctx + "-child", "ok");
    } catch (hash<ExceptionInfo> ex) {
        result(ctx + "-child", ex.err);
    }
    try {
        s.callFunction("w", True);
        result(ctx + "-child-static", "ok");
    } catch (hash<ExceptionInfo> ex) {
        result(ctx + "-child-static", ex.err);
    }
    result(ctx + "-child-state", OurVarMod::state());
}

# the host gives the module's variable that it imported to a child Program
{
    OurVarMod::reset();
    Program c(PO_NEW_STYLE);
    c.setSandboxManager(new SandboxManager());
    c.importGlobalVariable("OurVarMod::i", False);
    c.parse("sub w() { OurVarMod::i = 7; }\n", "chain");
    result("import-chain", try_call(c, "w"));
    Program cr(PO_NEW_STYLE);
    cr.importGlobalVariable("OurVarMod::i", True);
    cr.parse("sub w() { OurVarMod::i = 7; }\n", "chain-ro");
    result("import-chain-ro", try_call(cr, "w"));
    Program t(PO_NEW_STYLE);
    t.importGlobalVariable("OurVarMod::i", False);
    t.parse("sub w() { OurVarMod::i = 7; }\n", "trusted-chain");
    string r = try_call(t, "w");
    result("trusted-import-chain", r == "ok" && OurVarMod::i != 7 ? "unchanged" : r);
}

# the variables that are not a module's are not affected
our int host_var = 1;
{
    Program p(PO_NEW_STYLE);
    p.setSandboxManager(new SandboxManager());
    p.importGlobalVariable("host_var");
    p.parse("our int own = 1;\nsub w() { own = 2; }\nsub h() { host_var = 3; }\n"
        "class Own { public { static int s = 1; } }\nsub ws() { Own::s = 2; }\n", "own");
    result("own-var", try_call(p, "w"));
    result("own-static", try_call(p, "ws"));
    string r = try_call(p, "h");
    result("host-var", r == "ok" && host_var != 3 ? "unchanged" : r);
}
