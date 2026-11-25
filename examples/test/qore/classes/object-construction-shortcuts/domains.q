%modern
%requires DCMod

string sub frames(list<auto> cs) {
    list<string> l = ();
    foreach hash<auto> f in (cs) {
        l += sprintf("%s:%s:%s:%d", f.function, f.type, basename(f.file ?? ""), f.line);
    }
    return l.join(" < ");
}

const ProbeCode = "
string sub mutex_sub() { return DCMod::mkMutexSub(); }
string sub mutex_sub_ctor() { return DCMod::mkMutexSubCtor(); }
string sub socket_sub() { return DCMod::mkSocketSub(); }
string sub socket_sub_ctor() { return DCMod::mkSocketSubCtor(); }
string sub mutex() { return DCMod::mkMutex(); }
string sub keep() { object o = DCMod::keepMutexSub(); delete o; return \"deleted\"; }
";

hash<string, Program> callers;
foreach string label in ("unsandboxed", "sandboxed") {
    Program p(PO_NEW_STYLE);
    if (label == "sandboxed") {
        p.setSandboxManager(new SandboxManager());
    }
    p.loadModule("DCMod");
    p.setParseOptions(PO_LOCKDOWN);
    p.parse(ProbeCode, label);
    callers{label} = p;
}

foreach string f in ("mutex_sub", "mutex_sub_ctor", "socket_sub", "socket_sub_ctor", "mutex", "keep") {
    foreach hash<auto> c in (callers.pairIterator()) {
        for (int i = 0; i < 3; ++i) {
            string result;
            try {
                result = c.value.callFunction(f);
            } catch (hash<ExceptionInfo> ex) {
                result = sprintf("%s: %s @ %s", ex.err, ex.desc, frames(ex.callstack));
            }
            printf("%s %s %d: %s\n", c.key, f, i, result);
        }
    }
}
