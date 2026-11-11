%modern
# call_static_method(), call_static_method_args() and Program::callStaticMethod() call a private static method only
# for a caller with private access to its class: the class itself or a subclass (issue #5767); see
# call-static-method-private.qtest
public class P {
    private static string ps() { return "PRIV-only"; }
    static string sm(int i) { return "pub-sm"; }
    private static string sm(string s) { return "PRIV-sm"; }
    private:internal static string pi() { return "INT-only"; }
    static string pub() { return "pub"; }
    static string inside() { return call_static_method("P", "ps") + "/" + call_static_method("P", "sm", "x"); }
    static string insidei() { return call_static_method("P", "pi"); }
    static string insidep(Program p) { return p.callStaticMethod("Q", "q"); }
    # a closure created in the class keeps the class context when called outside it
    static code closure() { return string sub () { return call_static_method("P", "ps"); }; }
    # a call reference to the function is called in the context of its caller
    static code ref() { return \call_static_method(); }
    # a thread started by the class runs in its context
    static string thread() {
        Queue q();
        background q.push(call_static_method("P", "ps"));
        return q.get();
    }
    # call_function() is a builtin: the call it makes has no caller class
    static string viaCallFunction() { return call_function("call_static_method", "P", "ps"); }
}
class S inherits P {
    static string subc() { return call_static_method("P", "ps") + "/" + call_static_method("P", "sm", "x"); }
    static string subi() { return call_static_method("P", "pi"); }
}
class SP inherits P {
    static string viaProgram(Program p) { return p.callStaticMethod("P", "ps"); }
}
class X {
    static string unrelated() { return call_static_method("P", "ps"); }
}
string sub t(string label, code<string()> f) {
    string r;
    try { r = f(); } catch (hash<ExceptionInfo> ex) { r = ex.err; }
    return sprintf("%s=%s\n", label, r);
}
print(t("outside private-only", string sub () { return call_static_method("P", "ps"); }));
print(t("outside private overload", string sub () { return call_static_method("P", "sm", "x"); }));
print(t("outside public overload", string sub () { return call_static_method("P", "sm", 1); }));
print(t("outside args private", string sub () { return call_static_method_args("P", "ps"); }));
print(t("outside internal", string sub () { return call_static_method("P", "pi"); }));
print(t("outside public", string sub () { return call_static_method("P", "pub"); }));
print(t("inside", string sub () { return P::inside(); }));
print(t("inside internal", string sub () { return P::insidei(); }));
print(t("subclass", string sub () { return S::subc(); }));
print(t("subclass internal", string sub () { return S::subi(); }));
print(t("closure from the class", string sub () { return P::closure()(); }));
print(t("call reference from the class", string sub () { code f = P::ref(); return f("P", "ps"); }));
print(t("thread from the class", string sub () { return P::thread(); }));
print(t("call_function from the class", string sub () { return P::viaCallFunction(); }));
print(t("unrelated class", string sub () { return X::unrelated(); }));
Program p(PO_NEW_STYLE);
p.parse("class Q { private static string q() { return \"PRIV-q\"; } static string pq() { return \"pub-q\"; } }", "q");
print(t("Program::callStaticMethod from a subclass", string sub () { return SP::viaProgram(new Program()); }));
print(t("Program::callStaticMethod private", string sub () { return p.callStaticMethod("Q", "q"); }));
print(t("Program::callStaticMethodArgs private", string sub () { return p.callStaticMethodArgs("Q", "q"); }));
# a class imported into another Program is a copy: access is decided on the copy
Program ip(PO_NEW_STYLE | PO_NO_INHERIT_USER_CLASSES);
ip.importClass("P");
print(t("imported class from outside", string sub () { return ip.callStaticMethod("P", "ps"); }));
print(t("imported class from a subclass", string sub () { return SP::viaProgram(ip); }));
print(t("Program::callStaticMethod public", string sub () { return p.callStaticMethod("Q", "pq"); }));
