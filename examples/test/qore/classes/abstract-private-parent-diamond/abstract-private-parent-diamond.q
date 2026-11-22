%modern
# abstract-private-parent-diamond.q: an abstract declaration between a parent's private method and its implementation
# by a later parent class (qore#7206): B1 declares m() abstract over the private P1::m(), which implements it for B1,
# and X1 also reaches P1 through A1 and C1::m(); every call prints the method that it calls or the error that it
# raises, which every execution mode and qcc must print identically
string sub t(string l, code c) {
    try {
        return sprintf("%s: %y\n", l, c());
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s: %s\n", l, ex.err);
    }
}
class P1 {
    private string m() { return "P1::m"; }
    string viaP() { return m(); }
    string comP() { return call_object_method(self, "m"); }
    string refP() { code c = \m(); return c(); }
}
class A1 inherits P1 {
    string viaA() { return m(); }
}
class B1 inherits P1 {
    abstract string m();
    string viaB() { return m(); }
    string comB() { return call_object_method(self, "m"); }
    string refB() { code c = \m(); return c(); }
}
class C1 {
    string m() { return "C1::m"; }
    string viaC() { return m(); }
    string comC() { return call_object_method(self, "m"); }
}
class X1 inherits A1, B1, C1 {
    string viaX() { return m(); }
    string comX() { return call_object_method(self, "m"); }
    string refX() { code c = \m(); return c(); }
    bool canX() { return self.hasCallableMethod("m"); }
}
# a class outside the hierarchy
class K {
    static string k(X1 x) { return x.m(); }
    static string kref(X1 x) { code c = \x.m(); return c(); }
}

X1 x1();
print(t("outside", any sub () { return x1.m(); }));
print(t("outside call_object_method", any sub () { return call_object_method(x1, "m"); }));
print(t("outside call reference", any sub () { code c = \x1.m(); return c(); }));
print(t("outside hasCallableMethod", any sub () { return x1.hasCallableMethod("m"); }));
print(t("unrelated class", any sub () { return K::k(x1); }));
print(t("unrelated class call reference", any sub () { return K::kref(x1); }));
print(t("P1 code", any sub () { return x1.viaP(); }));
print(t("P1 call_object_method", any sub () { return x1.comP(); }));
print(t("P1 call reference", any sub () { return x1.refP(); }));
print(t("A1 code", any sub () { return x1.viaA(); }));
print(t("B1 code", any sub () { return x1.viaB(); }));
print(t("B1 call_object_method", any sub () { return x1.comB(); }));
print(t("B1 call reference", any sub () { return x1.refB(); }));
print(t("C1 code", any sub () { return x1.viaC(); }));
print(t("C1 call_object_method", any sub () { return x1.comC(); }));
print(t("X1 code", any sub () { return x1.viaX(); }));
print(t("X1 call_object_method", any sub () { return x1.comX(); }));
print(t("X1 call reference", any sub () { return x1.refX(); }));
print(t("X1 hasCallableMethod", any sub () { return x1.canX(); }));
# B1 alone: P1::m() implements the abstract declaration, privately
print(t("B1 instance", any sub () { B1 b(); return b.viaB(); }));
print(t("B1 instance outside", any sub () { B1 b(); return b.m(); }));
