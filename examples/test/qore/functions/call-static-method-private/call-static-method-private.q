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
print(t("unrelated class", string sub () { return X::unrelated(); }));
Program p(PO_NEW_STYLE);
p.parse("class Q { private static string q() { return \"PRIV-q\"; } static string pq() { return \"pub-q\"; } }", "q");
print(t("Program::callStaticMethod from a subclass", string sub () { return SP::viaProgram(new Program()); }));
print(t("Program::callStaticMethod private", string sub () { return p.callStaticMethod("Q", "q"); }));
print(t("Program::callStaticMethod public", string sub () { return p.callStaticMethod("Q", "pq"); }));
