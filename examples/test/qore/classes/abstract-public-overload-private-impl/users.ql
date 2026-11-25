# -*- mode: qore; indent-tabs-mode: nil -*-
# the classes and calls of abstract-public-overload-private-impl.qtest that inherit the classes of the namespace APO,
# which probe.q declares itself and module-user.q loads from the module; included by both

# the method that a class creates to implement G0's abstract m(int) with C5's variant, which it inherits privately,
# has G3's public m(bool), which a public path reaches (qore#7381)
class G6s inherits private:internal APO::C5, APO::G3 {
    string dyn(auto a) { object o = self; return o.m(a); }
    string com(auto a) { return call_object_method(self, "m", a); }
}
class G6p inherits private APO::C5, APO::G3 {
    string dyn(auto a) { object o = self; return o.m(a); }
    string com(auto a) { return call_object_method(self, "m", a); }
}
class G6h inherits private:hierarchy APO::C5, APO::G3 {}
class G6 inherits APO::C5, APO::G3 {
    string dyn(auto a) { object o = self; return o.m(a); }
    string com(auto a) { return call_object_method(self, "m", a); }
}
class G6r inherits APO::G3, private APO::C5 {}
# subclasses
class G7s inherits G6s {}
class G7p inherits G6p {}
class G7x inherits G6s { string m(string s) { return "G7x::m(string)"; } }
# private abstract variants: callable from the code of F0
class F6s inherits private:internal APO::C5, APO::F3 {
    string dyn(auto a) { object o = self; return o.m(a); }
}
class F6p inherits private APO::C5, APO::F3 {
    string dyn(auto a) { object o = self; return o.m(a); }
}
class F6 inherits APO::C5, APO::F3 {}
class F7s inherits F6s {}
# a class's own method has the public variants of the classes that a public path reaches after a privately
# inherited class
class H inherits private APO::C5, APO::P3 { string m(string s) { return "H::m(string)"; } }
class Hs inherits private:internal APO::C5, APO::P3 {
    string m(string s) { return "Hs::m(string)"; }
    string dyn(auto a) { object o = self; return o.m(a); }
}
# not callable from outside the class hierarchy: no public path, a private variant
class Hn inherits private APO::C5, private APO::P3 { string m(string s) { return "Hn::m(string)"; } }
class Hq inherits private APO::C5, APO::P3p { string m(string s) { return "Hq::m(string)"; } }
# C5 is reached only through K's private:internal inheritance
class L inherits APO::K, APO::P3 { string m(string s) { return "L::m(string)"; } }
# stays abstract: C5's m(int) is reached only through K's private:internal inheritance
class Lg inherits APO::K, APO::G3 {}
# the code of a public parent class reached after a private:internal parent class does not gain access to the class's
# other private:internal parent classes
class Y7 { string m(bool b) { return "Y7::m(bool)"; } }
class X7 { string m(float f) { return "X7::m(float)"; } }
class A7 {
    string m(int i) { return "A7::m(int)"; }
    string callFromA(auto a) { object o = self; return o.m(a); }
}
class Head7 inherits private:internal X7, A7, private:internal Y7 { string m(string s) { return "Head7::m(string)"; } }

string sub call(object o, auto arg) {
    try {
        return exists arg ? o.m(arg) : o.m();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub callm(object o, string meth, auto arg) {
    if (!o.hasCallableMethod(meth)) {
        return "-";
    }
    try {
        return call_object_method(o, meth, arg);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

const Args = (NOTHING, 1, "a", True);

foreach string cn in ("G6s", "G6p", "G6h", "G6", "G6r", "G7s", "G7p", "G7x", "F6s", "F6p", "F6", "F7s", "H", "Hs",
        "Hn", "Hq", "L", "Lg", "Head7") {
    object o;
    try {
        o = create_object(cn);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", cn, ex.err);
        continue;
    }
    printf("%s: %s\n", cn, (map sprintf("m(%s): %s", $1.type(), call(o, $1)), Args).join("; "));
    printf("%s cm: %s\n", cn, (map sprintf("m(%s): %s", $1.type(), callm(o, "cm", $1)), Args).join("; "));
    printf("%s dyn: %s\n", cn, (map sprintf("m(%s): %s", $1.type(), callm(o, "dyn", $1)), Args).join("; "));
    printf("%s com: %s\n", cn, (map sprintf("m(%s): %s", $1.type(), callm(o, "com", $1)), Args).join("; "));
    printf("%s call_object_method: %s\n", cn, (map sprintf("m(%s): %s", $1.type(), callm(o, "m", $1)),
        Args).join("; "));
    printf("%s callFromA: %s\n", cn, (map sprintf("m(%s): %s", $1.type(), callm(o, "callFromA", $1)),
        Args + 1.5).join("; "));
}

string sub r(code c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# calls with the class's type and through call references
{
    G6s gs();
    G6p gp();
    G7s g7();
    H h();
    printf("typed G6s: %s; %s\n", gs.m(True), gs.m(1));
    printf("typed G6p: %s; %s\n", gp.m(True), gp.m(1));
    printf("typed G7s: %s; %s\n", g7.m(True), g7.m(1));
    printf("typed H: %s; %s\n", h.m(True), h.m("a"));
    APO::G3 g3 = gs;
    printf("typed G6s as G3: %s; %s\n", g3.m(True), g3.m(1));
    code c = \gs.m();
    printf("call reference G6s: %s; %s\n", c(True), c(1));
    c = \gp.m();
    printf("call reference G6p: %s; %s\n", c(True), c(1));
    c = \h.m();
    printf("call reference H: %s; %s; %s\n", c(True), c("a"), r(string sub () { return c(1); }));
}
