#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# code of a class calls its own method (or the method of its own hierarchy) rather than a private method of a common
# parent class that an earlier parent class of the object reaches first (qore#6798); the test compares the output of
# each execution mode and of qcc executables with probe.expected

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

#! parses the given program in a child Program and runs its functions: a typed call is resolved when parsing
sub parse_check(string label, string src, list<string> funcs) {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
    try {
        p.parse(src, label);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
        return;
    }
    foreach string f in (funcs) {
        check(label + " " + f, any sub () { return p.callFunction(f); });
    }
}

# the diamond of the issue: X2 inherits A2, B22; P2 is reached through A2 first
class P2 {
    private string m() { return "P2::m"; }
    private static string sm() { return "P2::sm"; }
    private string m2(int i) { return "P2::m2"; }

    string pSelf() { return self.m(); }
}

class A2 inherits P2 {
    string aSelf() { return self.m(); }
}

class B22 inherits P2 {
    string m() { return "B22::m"; }
    static string sm() { return "B22::sm"; }
    string m2(int i) { return "B22::m2 " + i; }

    string selfDot() { return self.m(); }
    string selfCall() { return m(); }
    string selfArgs() { return self.m2(1); }
    string typedX2(X2 o) { return o.m(); }
    string typedB22(B22 o) { return o.m(); }
    string untyped(auto o) { return o.m(); }
    string selfRef() { code c = \self.m(); return c(); }
    string objRef(auto o) { code c = \o.m(); return c(); }
    string typedRef(X2 o) { code c = \o.m(); return c(); }
    string getCallRef() { code c = self.getCallReference("m"); return c(); }
    bool hasCallable() { return self.hasCallableMethod("m"); }
    string com() { return call_object_method(self, "m"); }
    string comArgs() { return call_object_method_args(self, "m2", (2,)); }
    string closureDot() { code c = string sub () { return self.m(); }; return c(); }
    string closureCall() { code c = string sub () { return m(); }; return c(); }
    string staticCall() { return sm(); }
    string staticDot() { return self.sm(); }
    string staticScoped() { return X2::sm(); }
    string staticTyped(X2 o) { return o.sm(); }
    string staticRef() { code c = \X2::sm(); return c(); }
    string staticCsm() { return call_static_method("X2", "sm"); }
    string other(auto o) { return o.m(); }
}

class X2 inherits A2, B22 {
    string xSelf() { return self.m(); }
}

# a subclass of X2 with a public override: it is called from the code of B22 as well
class Y2 inherits X2 {
    string m() { return "Y2::m"; }
}

# an object of a class that inherits P2 but not B22: the code of B22 calls P2's private method
class Z2 inherits P2 {}

# a class of another branch listed before B22 overrides the method publicly: it is called
class C7 inherits P2 {
    string m() { return "C7::m"; }
}
class X7 inherits A2, C7, B22 {}

# a class between B9 and P2 overrides the method
class M9 inherits P2 {
    string m() { return "M9::m"; }
}
class B9 inherits M9 {
    string bSelf() { return self.m(); }
    string bCall() { return m(); }
}
class X9 inherits A2, B9 {}

# C11's own hierarchy gives the method of Q11, its first parent class, which is not a subclass of P2
class Q11 {
    string m() { return "Q11::m"; }
}
class C11 inherits Q11, P2 {
    string cSelf() { return self.m(); }
}
class X11 inherits A2, C11 {}

# private:hierarchy method of the common parent class
class P3 {
    private:hierarchy string m() { return "P3::m"; }
}
class A3 inherits P3 {}
class B33 inherits P3 {
    string m() { return "B33::m"; }
    string bSelf() { return self.m(); }
    string bCom() { return call_object_method(self, "m"); }
}
class X3 inherits A3, B33 {}

# private:internal method of the common parent class: B44 cannot call it
class P4 {
    private:internal string m() { return "P4::m"; }
    string pSelf() { return self.m(); }
}
class A4 inherits P4 {}
class B44 inherits P4 {
    string m() { return "B44::m"; }
    string bSelf() { return self.m(); }
}
class X4 inherits A4, B44 {}

# the own method is private
class P5 {
    private string m() { return "P5::m"; }
}
class A5 inherits P5 {}
class B55 inherits P5 {
    private string m() { return "B55::m"; }
    string bSelf() { return self.m(); }
    string bRef() { code c = \self.m(); return c(); }
}
class X5 inherits A5, B55 {}

# the own method is private:internal
class P6 {
    private string m() { return "P6::m"; }
}
class A6 inherits P6 {}
class B66 inherits P6 {
    private:internal string m() { return "B66::m"; }
    string bSelf() { return self.m(); }
    string bCall() { return m(); }
}
class X6 inherits A6, B66 {}

# the policy-base example of the documentation (multiple_inheritance_order): unchanged
class Base {
    private string hook() { return "a"; }
    string call() { return hook(); }
}
class Leaf inherits Base {
    string doIt() { return call(); }
}
class SpBase inherits Base {
    private string hook() { return "b"; }
}
class DocA inherits Leaf, SpBase {}
class DocB inherits SpBase, Leaf {}

X2 x();
check("X2 B22 self.m()", \x.selfDot());
check("X2 B22 m()", \x.selfCall());
check("X2 B22 self.m2(1)", \x.selfArgs());
check("X2 B22 typed X2", any sub () { return x.typedX2(x); });
check("X2 B22 typed B22", any sub () { return x.typedB22(x); });
check("X2 B22 untyped", any sub () { return x.untyped(x); });
check("X2 B22 \\self.m()", \x.selfRef());
check("X2 B22 \\o.m() untyped", any sub () { return x.objRef(x); });
check("X2 B22 \\o.m() typed", any sub () { return x.typedRef(x); });
check("X2 B22 getCallReference()", \x.getCallRef());
check("X2 B22 hasCallableMethod()", \x.hasCallable());
check("X2 B22 call_object_method()", \x.com());
check("X2 B22 call_object_method_args()", \x.comArgs());
check("X2 B22 closure self.m()", \x.closureDot());
check("X2 B22 closure m()", \x.closureCall());
check("X2 B22 sm()", \x.staticCall());
check("X2 B22 self.sm()", \x.staticDot());
check("X2 B22 X2::sm()", \x.staticScoped());
check("X2 B22 o.sm() typed", any sub () { return x.staticTyped(x); });
check("X2 B22 \\X2::sm()", \x.staticRef());
check("X2 B22 call_static_method()", \x.staticCsm());
check("X2 P2 self.m()", \x.pSelf());
check("X2 A2 self.m()", \x.aSelf());
check("X2 X2 self.m()", \x.xSelf());
check("X2 outside m()", any sub () { return x.m(); });
check("X2 outside call_object_method()", any sub () { return call_object_method(x, "m"); });

Y2 y();
check("Y2 B22 self.m()", \y.selfDot());
check("Y2 B22 untyped", any sub () { return y.untyped(y); });
check("Y2 B22 typed X2", any sub () { return y.typedX2(y); });
check("Y2 B22 call_object_method()", \y.com());

Z2 z();
B22 b();
check("Z2 from B22 untyped", any sub () { return b.other(z); });
check("B22 B22 self.m()", \b.selfDot());

X7 x7();
check("X7 B22 self.m()", \x7.selfDot());
check("X7 B22 call_object_method()", \x7.com());
check("X7 outside m()", any sub () { return x7.m(); });

X9 x9();
check("X9 B9 self.m()", \x9.bSelf());
check("X9 B9 m()", \x9.bCall());

X11 x11();
check("X11 C11 self.m()", \x11.cSelf());
C11 c11();
check("C11 C11 self.m()", \c11.cSelf());

X3 x3();
check("X3 B33 self.m()", \x3.bSelf());
check("X3 B33 call_object_method()", \x3.bCom());

X4 x4();
check("X4 B44 self.m()", \x4.bSelf());
check("X4 P4 self.m()", \x4.pSelf());

X5 x5();
check("X5 B55 self.m()", \x5.bSelf());
check("X5 B55 \\self.m()", \x5.bRef());

X6 x6();
check("X6 B66 self.m()", \x6.bSelf());
check("X6 B66 m()", \x6.bCall());

check("doc A doIt()", any sub () { return (new DocA()).doIt(); });
check("doc B doIt()", any sub () { return (new DocB()).doIt(); });

# typed calls parsed in a child Program from the code of a class there
parse_check("child", "
class P { private string m() { return \"P::m\"; } }
class A inherits P {}
class B inherits P {
    string m() { return \"B::m\"; }
    string typed(X o) { return o.m(); }
    string selfDot() { return self.m(); }
}
class X inherits A, B {}
string sub typed() { X x(); return x.typed(x); }
string sub selfDot() { X x(); return x.selfDot(); }
", ("typed", "selfDot"));

# a typed call that the parser resolves: B's own method has the variant called, which P's private method has not
parse_check("child variant", "
class P { private string m() { return \"P::m\"; } }
class A inherits P {}
class B inherits P {
    string m(int i) { return \"B::m \" + i; }
    string typed(X o) { return o.m(1); }
}
class X inherits A, B {}
string sub typed() { X x(); return x.typed(x); }
", ("typed",));
