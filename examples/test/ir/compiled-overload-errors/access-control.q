%modern
# private and internal method variants resolved at run time are not visible outside their access scope in any execution
# mode (issues #5709 and #5717): static and normal methods, called from outside the class, from a subclass and from
# inside the class, through call references, and from a sandboxed child Program; see compiled-overload-errors.qtest
const ClassSrc = "
class C { public { int m = 1; } reference<int> get() { return \\m; } }
class P {
    static string sm(int i) { return \"pub-sm\"; }
    private static string sm(auto a) { return \"PRIV-sm\"; }
    static string si(int i) { return \"pub-si\"; }
    private:internal static string si(auto a) { return \"INT-si\"; }
    string nm(int i) { return \"pub-nm\"; }
    private string nm(auto a) { return \"PRIV-nm\"; }
    string ni(int i) { return \"pub-ni\"; }
    private:internal string ni(auto a) { return \"INT-ni\"; }
    static string insideS(C c) { return P::sm(c.get()) + \"/\" + P::si(c.get()); }
    string insideN(C c) { return nm(c.get()) + \"/\" + ni(c.get()); }
}
class S inherits P {
    static string subS(C c) { return P::sm(c.get()); }
    static string subSi(C c) { return P::si(c.get()); }
    string subN(C c) { return nm(c.get()); }
    string subNi(C c) { return ni(c.get()); }
}
string sub t(string label, code<string()> f) {
    string r;
    try { r = f(); } catch (hash<ExceptionInfo> ex) { r = ex.err; }
    return sprintf(\"%s=%s\\n\", label, r);
}
string sub calls() {
    C c();
    P p();
    S s();
    string rv;
    rv += t(\"outside static private\", string sub () { return P::sm(c.get()); });
    rv += t(\"outside static internal\", string sub () { return P::si(c.get()); });
    rv += t(\"outside private\", string sub () { return p.nm(c.get()); });
    rv += t(\"outside internal\", string sub () { return p.ni(c.get()); });
    rv += t(\"inside static\", string sub () { return P::insideS(c); });
    rv += t(\"inside\", string sub () { return p.insideN(c); });
    rv += t(\"subclass static private\", string sub () { return S::subS(c); });
    rv += t(\"subclass static internal\", string sub () { return S::subSi(c); });
    rv += t(\"subclass private\", string sub () { return s.subN(c); });
    rv += t(\"subclass internal\", string sub () { return s.subNi(c); });
    rv += t(\"static call reference\", string sub () { code f = \\P::sm(); return f(c.get()); });
    rv += t(\"method call reference\", string sub () { code f = \\p.nm(); return f(c.get()); });
    return rv;
}
";

#! returns the execution mode this process runs in (an AOT executable runs in the default mode)
enum<ExecMode> sub exec_mode() {
    foreach string arg in (QORE_ARGV) {
        *string mode = (arg =~ x/^--exec-mode=(.*)$/)[0];
        switch (mode) {
            case "ast": return EM_AST;
            case "ir": return EM_IR;
            case "jit": return EM_JIT;
            case "tiered": return EM_TIERED;
        }
    }
    return EM_TIERED;
}

# the same code in this Program, compiled by qcc for the AOT executable
class C { public { int m = 1; } reference<int> get() { return \m; } }
class P {
    static string sm(int i) { return "pub-sm"; }
    private static string sm(auto a) { return "PRIV-sm"; }
    static string si(int i) { return "pub-si"; }
    private:internal static string si(auto a) { return "INT-si"; }
    string nm(int i) { return "pub-nm"; }
    private string nm(auto a) { return "PRIV-nm"; }
    string ni(int i) { return "pub-ni"; }
    private:internal string ni(auto a) { return "INT-ni"; }
    static string insideS(C c) { return P::sm(c.get()) + "/" + P::si(c.get()); }
    string insideN(C c) { return nm(c.get()) + "/" + ni(c.get()); }
}
class S inherits P {
    static string subS(C c) { return P::sm(c.get()); }
    static string subSi(C c) { return P::si(c.get()); }
    string subN(C c) { return nm(c.get()); }
    string subNi(C c) { return ni(c.get()); }
}
string sub t(string label, code<string()> f) {
    string r;
    try { r = f(); } catch (hash<ExceptionInfo> ex) { r = ex.err; }
    return sprintf("%s=%s\n", label, r);
}
string sub calls() {
    C c();
    P p();
    S s();
    string rv;
    rv += t("outside static private", string sub () { return P::sm(c.get()); });
    rv += t("outside static internal", string sub () { return P::si(c.get()); });
    rv += t("outside private", string sub () { return p.nm(c.get()); });
    rv += t("outside internal", string sub () { return p.ni(c.get()); });
    rv += t("inside static", string sub () { return P::insideS(c); });
    rv += t("inside", string sub () { return p.insideN(c); });
    rv += t("subclass static private", string sub () { return S::subS(c); });
    rv += t("subclass static internal", string sub () { return S::subSi(c); });
    rv += t("subclass private", string sub () { return s.subN(c); });
    rv += t("subclass internal", string sub () { return s.subNi(c); });
    rv += t("static call reference", string sub () { code f = \P::sm(); return f(c.get()); });
    rv += t("method call reference", string sub () { code f = \p.nm(); return f(c.get()); });
    return rv;
}

sub main() {
    Program sandbox(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_LOCKDOWN);
    sandbox.setExecMode(exec_mode());
    sandbox.parse(ClassSrc, "access-control-sandbox");
    for (int i = 0; i < 3; ++i) {
        print(calls());
        print("sandbox:\n" + sandbox.callFunction("calls"));
    }
}
main();
