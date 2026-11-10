%modern
%no-child-restrictions

# the program of module-command-context.qtest: every case prints one line, which the test compares in every execution
# mode and as a qcc executable; the modules cmdctxaware and cmdctxlegacy (examples/test/module-cpp-api/module-command)
# must be in the module path

#! the Program code that issues a command to the Program it runs in, and to a Program that it is given
const IssueCode = "%modern
%requires CmdCtxUser
public sub issue_by_module(string mod, string cmd) {
    CmdCtxUser::issue(mod, cmd);
}
public sub issue_own(string mod, string cmd) {
    Program::issueModuleCmd(mod, cmd);
}
public sub issue_to(Program target, string mod, string cmd) {
    target.issueModuleCmd(mod, cmd);
}
";

#! returns what to print for an exception: its code, with the result that the thread and outside commands report
string sub describe(hash<ExceptionInfo> ex) {
    return ex.err =~ /^CMDCTX-(THREAD|OUTSIDE)$/ ? ex.err + "=" + (ex.desc =~ x/^(\w+)/)[0] : ex.err;
}

#! runs a command in the current Program and returns "ok" or the exception code
string sub issue(string mod, string cmd) {
    try {
        Program::issueModuleCmd(mod, cmd);
        return "ok";
    } catch (hash<ExceptionInfo> ex) {
        return describe(ex);
    }
}

#! returns the history that a module recorded (and clears it)
string sub history(string mod) {
    try {
        Program::issueModuleCmd(mod, "log");
        return "?";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err == "CMDCTX-LOG" ? (ex.desc =~ x/^([^:]*)/)[0] ?? "" : ex.err;
    }
}

#! creates a Program with the given parse options that has the issuing functions
Program sub make_program(int po = 0) {
    Program p(po);
    p.parse(IssueCode, "issue-code");
    return p;
}

#! runs a command in \a p, which issues it to itself, and returns "ok" or the exception code
string sub issue_in(Program p, string mod, string cmd) {
    try {
        p.callFunction("issue_own", mod, cmd);
        return "ok";
    } catch (hash<ExceptionInfo> ex) {
        return describe(ex);
    }
}

#! runs a command in \a p through module code, which issues it to its own Program, and returns "ok" or the exception code
string sub issue_via_module(Program p, string mod, string cmd) {
    try {
        p.callFunction("issue_by_module", mod, cmd);
        return "ok";
    } catch (hash<ExceptionInfo> ex) {
        return describe(ex);
    }
}

#! parses a directive in a new Program and returns "ok" or the exception code
string sub parse_cmd(string src) {
    try {
        Program p();
        p.parse(src, "directive");
        return "ok";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# trusted code
printf("01 trusted, own Program:      %s | %s\n", issue("cmdctxaware", "record"), history("cmdctxaware"));
{
    Program t();
    string result = "ok";
    try {
        t.issueModuleCmd("cmdctxaware", "record");
    } catch (hash<ExceptionInfo> ex) {
        result = ex.err;
    }
    printf("02 trusted, other Program:    %s | %s\n", result, history("cmdctxaware"));
}

# %module-cmd
printf("03 parse, no restriction:     %s | %s\n", parse_cmd("%module-cmd(cmdctxaware) record\n"),
    history("cmdctxaware"));
printf("04 parse, restricted before:  %s | %s\n", parse_cmd("%no-filesystem\n%module-cmd(cmdctxaware) record\n"),
    history("cmdctxaware"));
printf("05 parse, restricted after:   %s | %s\n", parse_cmd("%module-cmd(cmdctxaware) record\n%no-filesystem\n"),
    history("cmdctxaware"));

# restricted code
{
    Program r = make_program(PO_NO_FILESYSTEM);
    printf("06 restricted, record:        %s | %s\n", issue_in(r, "cmdctxaware", "record"), history("cmdctxaware"));
    printf("07 restricted, path:          %s\n", issue_in(r, "cmdctxaware", "path"));
    printf("08 restricted, code:          %s\n", issue_in(r, "cmdctxaware", "code"));
    printf("09 restricted, thread:        %s\n", issue_in(r, "cmdctxaware", "thread"));
    printf("10 restricted, nested:        %s | %s\n", issue_in(r, "cmdctxaware", "nested"),
        history("cmdctxaware"));
}
{
    Program r = make_program(PO_NO_UNCONTROLLED_APIS);
    printf("11 no-uncontrolled, code:     %s\n", issue_in(r, "cmdctxaware", "code"));
    printf("12 no-uncontrolled, path:     %s\n", issue_in(r, "cmdctxaware", "path"));
    printf("13 no-uncontrolled, thread:   %s\n", issue_in(r, "cmdctxaware", "thread"));
}
{
    Program r = make_program(PO_NO_EMBEDDED_LOGIC);
    printf("14 no-embedded-logic, code:   %s\n", issue_in(r, "cmdctxaware", "code"));
}
printf("15 trusted, code and path:    %s %s\n", issue("cmdctxaware", "code"), issue("cmdctxaware", "path"));
printf("16 trusted, thread:           %s\n", issue("cmdctxaware", "thread"));
printf("17 outside a handler:         %s\n", issue("cmdctxaware", "outside"));

# a restricted Program issues a command to a trusted Program: refused by the target's lack of restrictions
{
    Program r = make_program(PO_NO_FILESYSTEM);
    Program t();
    string result = "ok";
    try {
        r.callFunction("issue_to", t, "cmdctxaware", "record");
    } catch (hash<ExceptionInfo> ex) {
        result = ex.err;
    }
    printf("18 restricted to trusted:     %s | %s\n", result, history("cmdctxaware"));
}

# a module that does not declare that its handler honors the restrictions of the issuer
printf("19 legacy, trusted:           %s | %s\n", issue("cmdctxlegacy", "record"), history("cmdctxlegacy"));
{
    Program r = make_program(PO_NO_FILESYSTEM);
    printf("20 legacy, restricted:        %s | %s\n", issue_in(r, "cmdctxlegacy", "record"), history("cmdctxlegacy"));
}
printf("21 legacy, parse, plain:      %s | %s\n", parse_cmd("%module-cmd(cmdctxlegacy) record\n"),
    history("cmdctxlegacy"));
printf("22 legacy, parse, restricted: %s | %s\n", parse_cmd("%no-filesystem\n%module-cmd(cmdctxlegacy) record\n"),
    history("cmdctxlegacy"));
printf("23 legacy, parse, after:      %s | %s\n", parse_cmd("%module-cmd(cmdctxlegacy) record\n%no-filesystem\n"),
    history("cmdctxlegacy"));

# sandboxed code: a Program that a sandbox governs
{
    Program s = make_program();
    SandboxManager sm();
    s.setSandboxManager(sm);
    printf("24 sandboxed, record:         %s | %s\n", issue_in(s, "cmdctxaware", "record"), history("cmdctxaware"));
    printf("25 sandboxed, code:           %s\n", issue_in(s, "cmdctxaware", "code"));
    printf("26 sandboxed, path:           %s\n", issue_in(s, "cmdctxaware", "path"));
    printf("27 sandboxed, thread:         %s\n", issue_in(s, "cmdctxaware", "thread"));
    printf("28 sandboxed, legacy:         %s | %s\n", issue_in(s, "cmdctxlegacy", "record"), history("cmdctxlegacy"));
    Program t();
    string result = "ok";
    try {
        s.callFunction("issue_to", t, "cmdctxaware", "record");
    } catch (hash<ExceptionInfo> ex) {
        result = ex.err;
    }
    printf("29 sandboxed to trusted:      %s | %s\n", result, history("cmdctxaware"));
}

# module code that issues a command: the restrictions of the caller reach it as they reach any module code: through the
# sandbox that governs the caller (and a sandboxed Program), not through the parse options of the caller
{
    Program s = make_program();
    SandboxManager sm();
    s.setSandboxManager(sm);
    printf("30 sandboxed, via module:     %s | %s\n", issue_via_module(s, "cmdctxaware", "record"),
        (history("cmdctxaware") =~ x/(sandboxed=\d)/)[0]);
    printf("31 sandboxed, via module, code: %s\n", issue_via_module(s, "cmdctxaware", "code"));
    printf("32 sandboxed, via module, legacy: %s\n", issue_via_module(s, "cmdctxlegacy", "record"));
    # a parse option of the caller does not restrict module code (it is the sandbox that governs it)
    Program r = make_program(PO_NO_UNCONTROLLED_APIS);
    printf("33 restricted, via module:    %s | %s\n", issue_via_module(r, "cmdctxaware", "record"),
        (history("cmdctxaware") =~ x/(sandboxed=\d)/)[0]);
    printf("34 restricted, via module, code: %s\n", issue_via_module(r, "cmdctxaware", "code"));
}

#! loads a module whose source has a %module-cmd directive into a new Program, and returns "ok" or the exception code
string sub load_in_program(string name, int po, bool sandboxed) {
    try {
        Program p(po | PO_NEW_STYLE);
        if (sandboxed) {
            SandboxManager sm();
            p.setSandboxManager(sm);
        }
        p.parse("%requires " + name + "\n", "load");
        return "ok";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# the directive in the source of a module is library code: it is judged by the module's own Program (and the sandbox
# that governs the loader as for any library module), not by the parse options of the code that loads the module
printf("35 module directive, trusted loader:      %s | %s\n", load_in_program("CmdCtxDirectiveA", 0, False),
    history("cmdctxaware"));
printf("36 module directive, no-filesystem loader: %s | %s\n",
    load_in_program("CmdCtxDirectiveB", PO_NO_FILESYSTEM, False), history("cmdctxaware"));
printf("37 module directive, sandboxed loader:    %s | %s\n", load_in_program("CmdCtxDirectiveC", 0, True),
    history("cmdctxaware"));
