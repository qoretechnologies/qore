#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# the calls of class-context-private-method-skip/probe.q made by the code of classes imported into a child Program
# (Program::importClass(): copies of the classes with their class IDs) call the methods that they call in the source
# Program, for an object of the imported class and of a subclass declared in the child, and the typed calls parsed in
# the child resolve as they do in the source Program (qore#6759, qore#6671); the test compares the output of each
# execution mode and of qcc executables with imported.expected

%modern
%no-child-restrictions
%disable-warning non-existent-method-call

#! runs a call three times (a call site resolved at run time caches its method), giving its value or its error
string sub outcome(code c) {
    list<string> l = ();
    for (int i = 0; i < 3; ++i) {
        try {
            l += sprintf("%y", c());
        } catch (hash<ExceptionInfo> ex) {
            l += ex.err;
        }
    }
    # given once when every call gives the same outcome
    return (select l, $1 != l[0]) ? l.join(" | ") : l[0];
}

# an earlier parent class of the object has a private method of the name that code of the other parent class cannot
# call (qore#6759)
class A {
    private string m() { return "A::m"; }
    private static string sm() { return "A::sm"; }
}

class B2 {
    string m() { return "B2::m"; }
    static string sm() { return "B2::sm"; }

    string selfDot() { return self.m(); }
    string selfCall() { return m(); }
    string selfStatic() { return sm(); }
    string selfRef() { code c = \self.m(); return c(); }
    string untyped() { auto b = self; return b.m(); }
    string com() { return call_object_method(self, "m"); }
    string closureDot() { code c = string sub () { return self.m(); }; return c(); }
    string getCallRef() { code c = self.getCallReference("m"); return c(); }
    bool hasCallable() { return self.hasCallableMethod("m"); }
}

class X5 inherits A, B2 {
    string xCall() { return m(); }
    string xSelf() { return self.m(); }
    string xUntyped() { auto o = self; return o.m(); }
}

# a source subclass, the counterpart of the subclass declared in the child Program
class X7 inherits X5 {
}

# the methods of a private:internal parent class called by code of the class; the calls on an untyped value and by
# name are resolved at run time (qore#6671)
class Base {
    string bm() { return "Base::bm"; }
    static string sbm() { return "Base::sbm"; }
}

class Mid inherits private:internal Base {
    string call() { return bm(); }
    string callStatic() { return Base::sbm(); }
    string untyped() { auto o = self; return o.bm(); }
    string closureUntyped() { code c = string sub () { auto o = self; return o.bm(); }; return c(); }
    bool hasCallable() { return self.hasCallableMethod("bm"); }
}

class Sub1 inherits Mid {
}

const X5Methods = ("selfDot", "selfCall", "selfStatic", "selfRef", "untyped", "com", "closureDot", "getCallRef",
    "hasCallable", "xCall", "xSelf", "xUntyped");
const MidMethods = ("call", "callStatic", "untyped", "closureUntyped", "hasCallable");

Program p(PO_NEW_STYLE | PO_NO_INHERIT_USER_CLASSES);
p.importClass("X5");
p.importClass("Mid");
p.parse("
# subclasses of the imported classes declared in the child Program
class D inherits X5 {
    # calls parsed in the child in the class context of the subclass
    string dCall() { return m(); }
    string dTyped(X5 x) { return x.m(); }
    string dStatic() { return sm(); }
}

class S inherits Mid {
}

auto sub call(string cls, string meth) {
    return call_object_method(create_object(cls), meth);
}

auto sub d_call(string meth) {
    D d();
    return meth == \"dTyped\" ? d.dTyped(d) : call_object_method(d, meth);
}
", "child");

#! prints the outcome of a method for an object of a source class, of the imported class and of a child subclass
sub show(string meth, string src_cls, string src_sub, string cls, string child_sub) {
    printf("%s::%s(): source %s, source subclass %s, imported %s, child subclass %s\n", cls, meth,
        outcome(any sub () { return call_object_method(create_object(src_cls), meth); }),
        outcome(any sub () { return call_object_method(create_object(src_sub), meth); }),
        outcome(any sub () { return p.callFunction("call", cls, meth); }),
        outcome(any sub () { return p.callFunction("call", child_sub, meth); }));
}

foreach string meth in (X5Methods) {
    show(meth, "X5", "X7", "X5", "D");
}
foreach string meth in (MidMethods) {
    show(meth, "Mid", "Sub1", "Mid", "S");
}
foreach string meth in (("dCall", "dTyped", "dStatic")) {
    printf("child D::%s(): %s\n", meth, outcome(any sub () { return p.callFunction("d_call", meth); }));
}
