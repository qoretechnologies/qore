# probes of program-get-call-reference.qtest (qore#5863): Program::getCallReference() resolves a name in the
# Program, also a name that is the name of a method of the Program class

%modern

const Code = "
int sub run() { return 1; }
int sub getScriptName() { return 2; }
int sub callFunction() { return 3; }
int sub other() { return 4; }
class T { static int t() { return 5; } int m() { return 6; } }
";

string sub t(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# a method of the caller's class with the same name does not change the lookup either
class Caller {
    int run() { return -1; }
    code get(Program p, string name) { return p.getCallReference(name); }
}

string sub probes() {
    Program p(PO_NEW_STYLE);
    p.parse(Code, "code");
    Caller c();
    string rv;
    foreach string name in (("run", "getScriptName", "callFunction", "other", "T::t", "missing")) {
        rv += sprintf("%s: %s %s; ", name, t(auto sub () { return p.getCallReference(name)(); }),
            t(auto sub () { return c.get(p, name)(); }));
    }
    return rv + "\n";
}
