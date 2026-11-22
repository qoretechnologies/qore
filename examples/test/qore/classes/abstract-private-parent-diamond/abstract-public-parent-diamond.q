%modern
# abstract-public-parent-diamond.q: the control of abstract-private-parent-diamond.q with a public P1::m(), which
# implements B1's abstract declaration and is the first method of the name of X1 (qore#7206): every call calls it
string sub t(string l, code c) {
    try {
        return sprintf("%s: %y\n", l, c());
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s: %s\n", l, ex.err);
    }
}
class P1 {
    string m() { return "P1::m"; }
}
class A1 inherits P1 {}
class B1 inherits P1 {
    abstract string m();
    string viaB() { return m(); }
    string comB() { return call_object_method(self, "m"); }
}
class C1 {
    string m() { return "C1::m"; }
    string viaC() { return m(); }
}
class X1 inherits A1, B1, C1 {}
# an abstract method implemented only by another parent class: the class gets the implementation of that class
class B2 {
    abstract string m();
    string viaB() { return m(); }
}
class C2 {
    string m() { return "C2::m"; }
}
class X2 inherits B2, C2 {}

X1 x1();
print(t("outside", any sub () { return x1.m(); }));
print(t("outside call_object_method", any sub () { return call_object_method(x1, "m"); }));
print(t("outside call reference", any sub () { code c = \x1.m(); return c(); }));
print(t("B1 code", any sub () { return x1.viaB(); }));
print(t("B1 call_object_method", any sub () { return x1.comB(); }));
print(t("C1 code", any sub () { return x1.viaC(); }));
X2 x2();
print(t("sibling implementation outside", any sub () { return x2.m(); }));
print(t("sibling implementation call reference", any sub () { code c = \x2.m(); return c(); }));
print(t("sibling implementation B2 code", any sub () { return x2.viaB(); }));
print(t("sibling implementation call_object_method", any sub () { return call_object_method(x2, "m"); }));
