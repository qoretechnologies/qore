#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# the private method of a parent class is not called from another parent class of the same object (qore#6735); the
# test compares the output of each execution mode and of qcc executables with probe.expected

%modern
%no-child-restrictions
%disable-warning non-existent-method-call

#! runs a call three times (a call site resolved at run time caches its method), printing its value or its error
sub check(string label, code c) {
    list<string> l = ();
    for (int i = 0; i < 3; ++i) {
        try {
            l += sprintf("%y", c());
        } catch (hash<ExceptionInfo> ex) {
            l += ex.err;
        }
    }
    # printed once when every call gives the same outcome
    printf("%s: %s\n", label, (select l, $1 != l[0]) ? l.join(" | ") : l[0]);
}

#! parses the given classes in a child Program: a typed call is checked when parsing
sub parse_check(string label, string classes) {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
    try {
        p.parse(classes, label);
        printf("%s: parsed\n", label);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

class A {
    private string m() { return "A::m"; }
    private:internal string mi() { return "A::mi"; }
    private:hierarchy string mh() { return "A::mh"; }
    private static string sm() { return "A::sm"; }
    private string m2(string s) { return "A::m2 " + s; }
    private string m3(string s) { return "A::m3 " + s; }

    string fromA(object o) { return o.m(); }
    string fromAi(object o) { return o.mi(); }
    string fromAs(object o) { return o.sm(); }
    code refA(object o) { return \o.m(); }
}

# a sibling of A with a public method of the name
class B2 {
    string m() { return "B2::m"; }
    string m2(string s) { return "B2::m2 " + s; }

    string call(object o) { return o.m(); }
    string callh(object o) { return o.mh(); }
    string calli(object o) { return o.mi(); }
    string calls(object o) { return o.sm(); }
    string callTmpArg(object o) { return o.m2(sprintf("%s", "t")); }
    string callVarArg(object o) { string v = "v"; return o.m3(v); }
    any refCall(object o) { code c = \o.m(); return c(); }
    code ref(object o) { return \o.m(); }
    any com(object o) { return call_object_method(o, "m"); }
    any coma(object o) { return call_object_method_args(o, "m"); }
    code closure() { return any sub (object o) { return o.m(); }; }
    # a closure created here and run in a background thread; its result or error is returned through a Queue
    any backgroundCall(object o) {
        Queue q();
        background sub () {
            try {
                q.push({"value": o.m()});
            } catch (hash<ExceptionInfo> ex) {
                q.push({"err": ex.err});
            }
        }();
        hash<auto> h = q.get();
        if (h.err) {
            throw h.err;
        }
        return h.value;
    }
}

# a sibling of A without a method of the name
class B3 {
    string call(object o) { return o.m(); }
    any refCall(object o) { code c = \o.m(); return c(); }
    any com(object o) { return call_object_method(o, "m"); }
    code closure() { return any sub (object o) { return o.m(); }; }
}

class X5 inherits A, B2 {
    string own(object o) { return o.m(); }
    string ownInternal(object o) { return o.mi(); }
    code ownRef(object o) { return \o.m(); }
    code ownClosure() { return any sub (object o) { return o.m(); }; }
}

class X6 inherits A, B3 {}

class Y5 inherits X5 {
    string fromY5(object o) { return o.m(); }
}

class U {
    static string s(object o) { return o.m(); }
}

X5 x();
X6 x6();
Y5 y();

# calls from a sibling parent class: refused as by a typed call
check("sibling untyped", any sub () { return x.call(x); });
check("sibling untyped private:hierarchy", any sub () { return x.callh(x); });
check("sibling untyped private:internal", any sub () { return x.calli(x); });
check("sibling untyped static", any sub () { return x.calls(x); });
check("sibling untyped temporary arg", any sub () { return x.callTmpArg(x); });
check("sibling untyped variable arg", any sub () { return x.callVarArg(x); });
check("sibling ref called inside", any sub () { return x.refCall(x); });
check("sibling ref called outside", any sub () { return x.ref(x)(); });
check("sibling call_object_method", any sub () { return x.com(x); });
check("sibling call_object_method_args", any sub () { return x.coma(x); });
check("sibling closure", any sub () { return x.closure()(x); });
check("sibling background closure", any sub () { return x.backgroundCall(x); });
check("sibling on subclass object", any sub () { return y.call(y); });
check("sibling without method untyped", any sub () { return x6.call(x6); });
check("sibling without method ref", any sub () { return x6.refCall(x6); });
check("sibling without method call_object_method", any sub () { return x6.com(x6); });
check("sibling without method closure", any sub () { return x6.closure()(x6); });

# calls that may call the private method
check("A code", any sub () { return x.fromA(x); });
check("A code private:internal", any sub () { return x.fromAi(x); });
check("A code static", any sub () { return x.fromAs(x); });
check("A ref", any sub () { return x.refA(x)(); });
check("X5 code", any sub () { return x.own(x); });
check("X5 code private:internal", any sub () { return x.ownInternal(x); });
check("X5 ref", any sub () { return x.ownRef(x)(); });
check("X5 closure", any sub () { return x.ownClosure()(x); });
check("Y5 code", any sub () { return y.fromY5(y); });
check("Y5 code on X5 object", any sub () { return y.fromY5(x); });

# calls from outside the hierarchy
check("unrelated static method", any sub () { return U::s(x); });
check("outside untyped", any sub () { object o = x; return o.m(); });
check("outside untyped without public method", any sub () { object o = x6; return o.m(); });

# typed calls
const PrivA = "class A { private string m() { return \"A::m\"; } }\n";
const PubB2 = "class B2 { string m() { return \"B2::m\"; } }\n";
parse_check("typed from sibling", PrivA + "class B2 { string m() { return \"B2::m\"; } string c(X o) { return o.m(); } }
class X inherits A, B2 {}");
parse_check("typed from sibling without method", PrivA + "class B3 { string c(X o) { return o.m(); } }
class X inherits A, B3 {}");
parse_check("typed from own class", PrivA + PubB2 + "class X inherits A, B2 { string c(X o) { return o.m(); } }");
parse_check("typed from private class", PubB2 + "class A { private string m() { return \"A::m\"; } string c(X o) { return o.m(); } }
class X inherits A, B2 {}");
parse_check("typed from subclass", PrivA + PubB2 + "class X inherits A, B2 {}
class Y inherits X { string c(X o) { return o.m(); } }");
parse_check("typed from outside", PrivA + PubB2 + "class X inherits A, B2 {}
string sub c(X o) { return o.m(); }");
