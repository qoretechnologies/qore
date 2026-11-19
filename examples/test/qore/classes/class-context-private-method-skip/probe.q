#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# code of a parent class calls its own method when an earlier parent class of the object has a private method of the
# name (qore#6759); the test compares the output of each execution mode and of qcc executables with probe.expected

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
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
    try {
        p.parse(src, label);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
        return;
    }
    foreach string f in (funcs) {
        check(label + " " + f, any sub () { return p.callFunction(f); });
    }
}

class A {
    private string m() { return "A::m"; }
    private static string sm() { return "A::sm"; }
    private string m2(int i) { return "A::m2"; }

    string aSelf() { return self.m(); }
    string aCall() { return m(); }
    string aStatic() { return sm(); }
    string aRef() { code c = \self.m(); return c(); }
    string aCom() { return call_object_method(self, "m"); }
}

class B2 {
    string m() { return "B2::m"; }
    static string sm() { return "B2::sm"; }
    string m2(int i) { return "B2::m2 " + i; }

    string selfDot() { return self.m(); }
    string selfCall() { return m(); }
    string selfArgs() { return self.m2(1); }
    string selfStatic() { return sm(); }
    string selfStaticDot() { return self.sm(); }
    string selfRef() { code c = \self.m(); return c(); }
    string selfMethodRef() { code c = \m(); return c(); }
    string typedB2(B2 b) { return b.m(); }
    string typedStatic(B2 b) { return b.sm(); }
    string untyped(auto b) { return b.m(); }
    string untypedRef(auto b) { code c = \b.m(); return c(); }
    string com() { return call_object_method(self, "m"); }
    string comArgs() { return call_object_method_args(self, "m"); }
    string closureDot() { code c = string sub () { return self.m(); }; return c(); }
    string closureCall() { code c = string sub () { return m(); }; return c(); }
    string background() {
        Queue q();
        code c = sub () {
            try {
                q.push(self.m());
            } catch (hash<ExceptionInfo> ex) {
                q.push(ex.err);
            }
        };
        background c();
        return q.get();
    }
    string getCallRef() { code c = self.getCallReference("m"); return c(); }
    bool hasCallable() { return self.hasCallableMethod("m"); }
}

class X5 inherits A, B2 {
    string xCall() { return m(); }
    string xSelf() { return self.m(); }
    string xStatic() { return sm(); }
}

class X7 inherits X5 {
    string x7Call() { return m(); }
    string x7Self() { return self.m(); }
}

# a public override in the object's class is called by everyone
class X8 inherits A, B2 {
    string m() { return "X8::m"; }
}

# a private method of the caller's own hierarchy is called
class C1 {
    private string m() { return "C1::m"; }
}

class B4 inherits C1 {
    string b4Self() { return self.m(); }
    string b4Call() { return m(); }
}

class X10 inherits A, B4 {
}

# a private override hides the public method of its parent class also from another parent class of the object
class P {
    string m() { return "P::m"; }
}

class AP inherits P {
    private string m() { return "AP::m"; }
}

class B3 {
    string b3Self() { return self.m(); }
}

class X9 inherits AP, B3 {
}

# a private:internal method of an earlier parent class is not found from another parent class
class AI {
    private:internal string m() { return "AI::m"; }
}

class XI inherits AI, B2 {
}

# a private:hierarchy method of an earlier parent class is skipped as a private method is
class AH {
    private:hierarchy string m() { return "AH::m"; }
}

class XH inherits AH, B2 {
}

# with no method of the name that the class context can call, the call raises the error that it raised before; a
# method gate is not called in place of the private method
class B5 {
    auto methodGate(string n) { return "B5::methodGate " + n; }
    string b5Self() { return self.m(); }
    bool b5HasCallable() { return self.hasCallableMethod("m"); }
    # a call reference by name to the method that a call of the name makes, which is refused as the call is
    any b5Ref() { code c = self.getCallReference("m"); return c(); }
    any b5ScopedRef() { code c = self.getCallReference("X11::m"); return c(); }
}

class X11 inherits A, B5 {
}

# the method of the name that the class context can call is in a later parent class
class B6 {
    string b6Self() { return self.m(); }
    any b6Ref() { code c = self.getCallReference("m"); return c(); }
}

class X12 inherits A, B6, B2 {
}

# the functional operators over an iterator object call the iterator methods that the class context can call
class AIt {
    private bool next() { return False; }
    private auto getValue() { return "AIt::getValue"; }
}

class BIt inherits AbstractIterator {
    private {
        int i = 0;
    }

    bool next() { return ++i <= 2; }
    auto getValue() { return "BIt " + i; }
    bool valid() { return i > 0 && i <= 2; }

    list<auto> mapSelf() {
        i = 0;
        return map $1, self;
    }

    list<auto> selectSelf() {
        i = 0;
        return select self, True;
    }
}

class XIt inherits AIt, BIt {
}

# a private abstract method implemented by a subclass is called from the class that declares it
class PB {
    private abstract string pa();
    string run() { return pa(); }
    string runSelf() { return self.pa(); }
}

class PI inherits PB {
    private string pa() { return "PI::pa"; }
}

X5 x();
check("B2 self.m()", any sub () { return x.selfDot(); });
check("B2 m()", any sub () { return x.selfCall(); });
check("B2 self.m2(1)", any sub () { return x.selfArgs(); });
check("B2 sm()", any sub () { return x.selfStatic(); });
check("B2 self.sm()", any sub () { return x.selfStaticDot(); });
check("B2 \\self.m()", any sub () { return x.selfRef(); });
check("B2 \\m()", any sub () { return x.selfMethodRef(); });
check("B2 B2 b.m()", any sub () { return x.typedB2(x); });
check("B2 B2 b.sm()", any sub () { return x.typedStatic(x); });
check("B2 auto b.m()", any sub () { return x.untyped(x); });
check("B2 \\b.m()", any sub () { return x.untypedRef(x); });
check("B2 call_object_method()", any sub () { return x.com(); });
check("B2 call_object_method_args()", any sub () { return x.comArgs(); });
check("B2 closure self.m()", any sub () { return x.closureDot(); });
check("B2 closure m()", any sub () { return x.closureCall(); });
check("B2 background closure", any sub () { return x.background(); });
check("B2 getCallReference()", any sub () { return x.getCallRef(); });
check("B2 hasCallableMethod()", any sub () { return x.hasCallable(); });
check("A self.m()", any sub () { return x.aSelf(); });
check("A m()", any sub () { return x.aCall(); });
check("A sm()", any sub () { return x.aStatic(); });
check("A \\self.m()", any sub () { return x.aRef(); });
check("A call_object_method()", any sub () { return x.aCom(); });
check("X5 m()", any sub () { return x.xCall(); });
check("X5 self.m()", any sub () { return x.xSelf(); });
check("X5 sm()", any sub () { return x.xStatic(); });
check("outside x.m()", any sub () { return x.m(); });
check("outside B2 b.m()", any sub () { B2 b = x; return b.m(); });

X7 x7();
check("X7 m()", any sub () { return x7.x7Call(); });
check("X7 self.m()", any sub () { return x7.x7Self(); });
check("X7 B2 self.m()", any sub () { return x7.selfDot(); });
check("X7 B2 auto b.m()", any sub () { return x7.untyped(x7); });

X8 x8();
check("X8 A m()", any sub () { return x8.aCall(); });
check("X8 B2 self.m()", any sub () { return x8.selfDot(); });
check("X8 B2 m()", any sub () { return x8.selfCall(); });
check("X8 outside", any sub () { return x8.m(); });

X10 x10();
check("X10 B4 self.m()", any sub () { return x10.b4Self(); });
check("X10 B4 m()", any sub () { return x10.b4Call(); });

X9 x9();
check("X9 B3 self.m()", any sub () { return x9.b3Self(); });

XI xi();
check("XI B2 self.m()", any sub () { return xi.selfDot(); });
check("XI B2 m()", any sub () { return xi.selfCall(); });

XH xh();
check("XH B2 self.m()", any sub () { return xh.selfDot(); });
check("XH B2 m()", any sub () { return xh.selfCall(); });

X11 x11();
check("X11 B5 self.m()", any sub () { return x11.b5Self(); });
check("X11 B5 hasCallableMethod()", any sub () { return x11.b5HasCallable(); });
check("X11 B5 getCallReference()", any sub () { return x11.b5Ref(); });
check("X11 B5 getCallReference() scoped", any sub () { return x11.b5ScopedRef(); });

X12 x12();
check("X12 B6 self.m()", any sub () { return x12.b6Self(); });
check("X12 B6 getCallReference()", any sub () { return x12.b6Ref(); });
check("X12 B2 self.m()", any sub () { return x12.selfDot(); });

PI pi();
check("PB pa()", any sub () { return pi.run(); });
check("PB self.pa()", any sub () { return pi.runSelf(); });

XIt xit();
check("XIt BIt map", any sub () { return xit.mapSelf(); });
check("XIt BIt select", any sub () { return xit.selectSelf(); });

# an abstract method of the class context implemented by the private method of another parent class
parse_check("abstract", "class A { private string m() { return \"A::m\"; } } "
    "class B { abstract string m(); string c() { return self.m(); } } "
    "class X inherits A, B {} "
    "string sub c() { X x(); return x.c(); }", ("c",));
parse_check("abstract typed", "class A { private string m() { return \"A::m\"; } } "
    "class B { abstract string m(); string t(X x) { return x.m(); } } "
    "class X inherits A, B {} "
    "string sub t() { X x(); return x.t(x); }", ("t",));
# an abstract method of the class context implemented by a public method of the object's class
parse_check("abstract override", "class A { private string m() { return \"A::m\"; } } "
    "class B { abstract string m(); string c() { return self.m(); } } "
    "class X inherits A, B { string m() { return \"X::m\"; } } "
    "string sub c() { X x(); return x.c(); }", ("c",));

# typed calls on the object's class resolved when parsing in the class context of another parent class
parse_check("typed", "class A { private string m() { return \"A::m\"; } "
    "private static string sm() { return \"A::sm\"; } } "
    "class B2 { string m() { return \"B2::m\"; } static string sm() { return \"B2::sm\"; } "
    "string t(X5 x) { return x.m(); } string ts(X5 x) { return x.sm(); } string sc() { return X5::sm(); } "
    "string r(X5 x) { code c = \\x.m(); return c(); } } "
    "class X5 inherits A, B2 { string xc(X5 x) { return x.m(); } } "
    "string sub t() { X5 x(); return x.t(x); } "
    "string sub ts() { X5 x(); return x.ts(x); } "
    "string sub sc() { X5 x(); return x.sc(); } "
    "string sub r() { X5 x(); return x.r(x); } "
    "string sub xc() { X5 x(); return x.xc(x); }", ("t", "ts", "sc", "r", "xc"));
# the method that the class context can call is a static method
parse_check("typed static", "class A { private string m() { return \"A::m\"; } } "
    "class B5 { static string m() { return \"B5::m static\"; } string t(X5 o) { return o.m(); } "
    "string u(object o) { return o.m(); } } "
    "class X5 inherits A, B5 {} "
    "string sub t() { X5 x(); return x.t(x); } "
    "string sub u() { X5 x(); return x.u(x); }", ("t", "u"));
# with no method that the class context can call, a typed call fails to parse as before
parse_check("typed none", "class A { private string m() { return \"A::m\"; } } "
    "class B3 { string t(X5 x) { return x.m(); } } "
    "class X5 inherits A, B3 {}", ());
# a diamond: a private override in the first parent class hides the method of the common parent class for code of the
# first parent class only; code of the other parent class calls the common parent class's method, also through a call
# reference by name
parse_check("diamond call reference", "class P { string m() { return \"P::m\"; } } "
    "class AP2 inherits P { private string m() { return \"AP2::m\"; } } "
    "class BP inherits P { string r() { return self.getCallReference(\"m\")(); } } "
    "class XP inherits AP2, BP {} "
    "string sub r() { XP x(); return x.r(); }", ("r",));
