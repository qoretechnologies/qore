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

# a public method reached through private inheritance in the earlier branch counts as a private method: code of B1
# calls its own method, as a caller outside the class hierarchy does
class P1 {
    string m() { return "P1::m"; }
}
class A1 inherits private P1 {}
class B1 inherits P1 {
    string m() { return "B1::m"; }
    string bSelf() { return self.m(); }
    string bUntyped(auto o) { return o.m(); }
    string bTyped(X1 o) { return o.m(); }
}
class X1 inherits A1, B1 {}
# the public method is reached through a public path before B1 as well: it is called
class C1 inherits P1 {}
class X1c inherits A1, C1, B1 {}

# B inherits P privately, A publicly
class PB { private string m() { return "PB::m"; } private static string sm() { return "PB::sm"; } }
class AB inherits PB {}
class BB inherits private PB {
    string m() { return "BB::m"; }
    static string sm() { return "BB::sm"; }
    string bSelf() { return self.m(); }
    string bStatic() { return sm(); }
    code bClosure() { return string sub () { return self.m(); }; }
    code bRef() { return \self.m(); }
}
class XB inherits AB, BB {}
# the object's class inherits B privately
class XBp inherits AB, private BB { string xc() { return bSelf(); } }
# the object's class inherits B with private:internal: the code of B keeps the documented order
class XBi inherits AB, private:internal BB { string xc() { return bSelf(); } }

# overloads: B's own method has only m(int); P's private method has m()
class PV { private string m() { return "PV::m()"; } }
class AV inherits PV {}
class BV inherits PV {
    string m(int i) { return "BV::m(int)"; }
    string c0() { return self.m(); }
    string c1() { return self.m(1); }
    string u0(auto o) { return o.m(); }
}
class XV inherits AV, BV {}
# overloads reversed: B's own method has m(); P's private method has m(int)
class PW { private string m(int i) { return "PW::m(int)"; } }
class AW inherits PW {}
class BW inherits PW {
    string m() { return "BW::m()"; }
    string c0() { return self.m(); }
    string c1() { return self.m(1); }
}
class XW inherits AW, BW {}

# P reached through two branches before B; P's parent has a public method reached through another branch
class PP5d { string m() { return "PP5d::m"; } }
class P5d inherits PP5d { private string m() { return "P5d::m"; } }
class A5d inherits P5d {}
class C5d inherits P5d {}
class D5d inherits PP5d {}
class B5d inherits P5d {
    string m() { return "B5d::m"; }
    string bSelf() { return self.m(); }
}
class X5d inherits A5d, C5d, B5d {}
class X5db inherits A5d, D5d, B5d {}

# multi-level diamond: P reached deeper in both branches, the object inherits a subclass of B
class P6d { private string m() { return "P6d::m"; } }
class Q6d inherits P6d {}
class A6d inherits Q6d {}
class R6d inherits P6d {}
class B6d inherits R6d {
    string m() { return "B6d::m"; }
    string bSelf() { return self.m(); }
}
class C6d inherits B6d {}
class X6d inherits A6d, C6d {}

# B's own hierarchy gives a private method of its other parent class, QQ, listed before P
class PQ { private string m() { return "PQ::m"; } }
class AQ inherits PQ {}
class QQ { private string m() { return "QQ::m"; } }
class BQ inherits QQ, PQ {
    string bSelf() { return self.m(); }
    string bUntyped(auto o) { return o.m(); }
    string bViaA(AQ o) { return o.m(); }
}
class CQ { string m() { return "CQ::m"; } }
class XQ inherits AQ, BQ {}
class XQ3 inherits AQ, BQ, CQ {}

# an abstract method of B does not override P's private method; the subclass implements it
class PA { private string m() { return "PA::m"; } }
class AA inherits PA {}
class BA inherits PA {
    abstract string m();
    string bSelf() { return self.m(); }
}
class XA inherits AA, BA {
    string m() { return "XA::m"; }
}

# an unrelated earlier branch has a private method of the name that the code of B cannot call: it is skipped, and the
# rule applies to the method found after it
class CU { private string m() { return "CU::m"; } private static string sm() { return "CU::sm"; } }
class AU inherits CU {}
class AUd inherits AU {}
class PU1 { private string m() { return "PU1::m"; } private static string sm() { return "PU1::sm"; } }
class DU1 inherits PU1 {}
class BU1 inherits PU1 {
    string m() { return "BU1::m"; }
    static string sm() { return "BU1::sm"; }
    string bSelf() { return self.m(); }
    string bUntyped(auto o) { return o.m(); }
    string bTyped(XU1 o) { return o.m(); }
    string bStatic() { return XU1::sm(); }
    string bCsm() { return call_static_method("XU1", "sm"); }
    string bCom() { return call_object_method(self, "m"); }
    string bRef() { code c = self.getCallReference("m"); return c(); }
}
class XU1 inherits AU, DU1, BU1 {}
# the unrelated private method between the branch reaching P and B
class XU1b inherits DU1, AU, BU1 {}
# the unrelated private method deeper in the first branch
class XU1c inherits AUd, DU1, BU1 {}
class XU1n inherits DU1, BU1 {}
# the public method of P reached through private inheritance before B
class PU2 { string m() { return "PU2::m"; } }
class DU2 inherits private PU2 {}
class BU2 inherits PU2 {
    string m() { return "BU2::m"; }
    string bSelf() { return self.m(); }
    string bUntyped(auto o) { return o.m(); }
    string bTyped(XU2 o) { return o.m(); }
}
class XU2 inherits AU, DU2, BU2 {}

# classes inherited by the classes of a child Program
public class PC { private string m() { return "PC::m"; } }
public class AC inherits PC {}
public class QC { private string m() { return "QC::m"; } }
public class BQC inherits QC, PC {
    string bSelf() { return self.m(); }
    string bUntyped(auto o) { return o.m(); }
}
public class BC inherits PC {
    string m() { return "BC::m"; }
    string bSelf() { return self.m(); }
    string bUntyped(auto o) { return o.m(); }
}
public class PPub { string m() { return "PPub::m"; } }
public class APub inherits private PPub {}
public class BPub inherits PPub {
    string m() { return "BPub::m"; }
    string bSelf() { return self.m(); }
}

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

X1 x1();
check("X1 B1 self.m() (public P1::m through private A1)", \x1.bSelf());
check("X1 B1 untyped", any sub () { return x1.bUntyped(x1); });
check("X1 B1 typed X1", any sub () { return x1.bTyped(x1); });
check("X1 outside m()", any sub () { return x1.m(); });
X1c x1c();
check("X1c B1 self.m() (P1 public through C1 too)", \x1c.bSelf());
check("X1c outside m()", any sub () { return x1c.m(); });

XB xb();
check("XB BB self.m() (BB inherits PB privately)", \xb.bSelf());
check("XB BB sm()", \xb.bStatic());
check("XB BB closure called outside", xb.bClosure());
check("XB BB \\self.m() called outside", xb.bRef());
XBp xbp();
check("XBp (private BB) BB self.m()", \xbp.xc());
check("XBp outside m()", any sub () { return xbp.m(); });
XBi xbi();
check("XBi (private:internal BB) BB self.m()", \xbi.xc());

XV xv();
check("XV BV self.m()", \xv.c0());
check("XV BV self.m(1)", \xv.c1());
check("XV BV untyped m()", any sub () { return xv.u0(xv); });
XW xw();
check("XW BW self.m()", \xw.c0());
check("XW BW self.m(1)", \xw.c1());

X5d x5d();
check("X5d B5d self.m() (P5d through two branches)", \x5d.bSelf());
X5db x5db();
check("X5db B5d self.m() (PP5d through D5d)", \x5db.bSelf());
check("X5db outside m()", any sub () { return x5db.m(); });

X6d x6d();
check("X6d B6d self.m() (multi-level)", \x6d.bSelf());

XQ xq();
check("XQ BQ self.m() (QQ first in BQ)", \xq.bSelf());
check("XQ BQ untyped", any sub () { return xq.bUntyped(xq); });
check("XQ BQ typed AQ", any sub () { return xq.bViaA(xq); });
XQ3 xq3();
check("XQ3 BQ self.m()", \xq3.bSelf());

XA xa();
check("XA BA self.m() (abstract in BA)", \xa.bSelf());

XU1 xu1();
check("XU1 (CU::m first) BU1 self.m()", \xu1.bSelf());
check("XU1 BU1 untyped", any sub () { return xu1.bUntyped(xu1); });
check("XU1 BU1 typed XU1", any sub () { return xu1.bTyped(xu1); });
check("XU1 BU1 XU1::sm()", \xu1.bStatic());
check("XU1 BU1 call_static_method()", \xu1.bCsm());
check("XU1 BU1 call_object_method()", \xu1.bCom());
check("XU1 BU1 getCallReference()", \xu1.bRef());
check("XU1 outside m()", any sub () { return xu1.m(); });
XU1b xu1b();
check("XU1b (CU::m between) BU1 self.m()", \xu1b.bSelf());
XU1c xu1c();
check("XU1c (CU::m deeper) BU1 self.m()", \xu1c.bSelf());
XU1n xu1n();
check("XU1n (no CU) BU1 self.m()", \xu1n.bSelf());
XU2 xu2();
check("XU2 (CU::m first) BU2 self.m()", \xu2.bSelf());
check("XU2 BU2 untyped", any sub () { return xu2.bUntyped(xu2); });
check("XU2 BU2 typed XU2", any sub () { return xu2.bTyped(xu2); });
check("XU2 outside m()", any sub () { return xu2.m(); });

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

# classes of this Program inherited by classes of a child Program
parse_check("child inherits", "
class CQ inherits AC, BQC {}
class CB inherits AC, BC {}
class CPub inherits APub, BPub {}
string sub cq() { CQ o(); return o.bSelf(); }
string sub cqu() { CQ o(); return o.bUntyped(o); }
string sub cb() { CB o(); return o.bSelf(); }
string sub cbu() { CB o(); return o.bUntyped(o); }
string sub cpub() { CPub o(); return o.bSelf(); }
", ("cq", "cqu", "cb", "cbu", "cpub"));
