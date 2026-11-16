#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a method call looks its method up before it evaluates its arguments, as the AST interpreter does: a private or
# missing method raises its error first, a deleted object of an overloaded method names the variant resolved at parse
# time, and a qualified call through self (Base::m()) is not looked up unless its method is abstract (issue #5824);
# prints what each case evaluates and raises

%modern

auto sub arg(string name) {
    printf("  arg %s\n", name);
    return 1;
}

string sub string_arg(string name) {
    printf("  arg %s\n", name);
    return "n";
}

class C {
    public {
        code member_closure = sub (auto a) { printf("  member closure called\n"); };
        int value = 1;
    }

    private p(auto a) {
        printf("  C::p() called\n");
    }

    m(int a) {
        printf("  C::m(int) called\n");
    }

    m(string a) {
        printf("  C::m(string) called\n");
    }

    n(auto a) {
        printf("  C::n() called\n");
    }

    #! a private method called from the class
    callPrivate() {
        object o = self;
        o.p(arg("private from the class"));
    }
}

#! a subclass that overrides a public method with a private one
class Parent {
    m(auto a) {
        printf("  Parent::m() called\n");
    }
}

class PrivateChild inherits Parent {
    private m(auto a) {
        printf("  PrivateChild::m() called\n");
    }
}

class Child inherits Parent {
    m(auto a) {
        printf("  Child::m() called\n");
    }
}

#! qualified calls of a base class method through self, which are not virtual
class QBase {
    m(auto a) {
        printf("  QBase::m() called\n");
    }
}

class QChild inherits QBase {
    m(auto a) {
        printf("  QChild::m() called\n");
        QBase::m(arg("qualified"));
    }

    del() {
        delete self;
        QBase::m(arg("qualified, deleted"));
    }
}

class QGrandChild inherits QChild {
}

#! calls of an abstract method, which are looked up by name on the object's class, also when they are qualified
class Abs {
    abstract a(int x);

    callQualified() {
        Abs::a(arg("qualified abstract"));
    }

    callQualifiedLiteral() {
        Abs::a(1);
    }

    callUnqualified() {
        a(arg("unqualified abstract"));
    }
}

class AbsPrivate inherits Abs {
    private a(int x) {
        printf("  AbsPrivate::a() called\n");
    }
}

class AbsPublic inherits Abs {
    a(int x) {
        printf("  AbsPublic::a() called\n");
    }
}

#! a concrete base method redeclared as abstract with the same signature: a qualified call is looked up by name
class ConcreteBase {
    c() {
        printf("  ConcreteBase::c() called\n");
    }

    b(int x) {
        printf("  ConcreteBase::b() called\n");
    }
}

class Redeclared inherits ConcreteBase {
    abstract c();
    abstract b(int x);

    callNoArgs() {
        Redeclared::c();
    }

    callArgs() {
        Redeclared::b(arg("redeclared abstract"));
    }

    callDeleted() {
        delete self;
        Redeclared::b(arg("redeclared abstract, deleted"));
    }
}

class RedeclaredImpl inherits Redeclared {
    c() {
        printf("  RedeclaredImpl::c() called\n");
    }

    b(int x) {
        printf("  RedeclaredImpl::b() called\n");
    }
}

#! a method with abstract and concrete variants: no abstract variant is bound to a qualified call
class MixedAbs {
    abstract a(int x);
    abstract a(string s);
}

class MixedMid inherits MixedAbs {
    a(string s) {
        printf("  MixedMid::a(string) called\n");
    }

    callInt() {
        MixedMid::a(arg("mixed variants"));
    }

    callString() {
        MixedMid::a("s");
    }
}

class MixedImpl inherits MixedMid {
    a(int x) {
        printf("  MixedImpl::a(int) called\n");
    }
}

#! abstract self calls in default argument values, which are serialized as inline expressions in programs compiled
#! with qcc
class DefaultBase {
    int c(int x) {
        return -x;
    }
}

class DefaultRedeclared inherits DefaultBase {
    abstract int c(int x);

    int qualifiedDefault(int x = DefaultRedeclared::c(3)) {
        return x;
    }

    int unqualifiedDefault(int x = c(2)) {
        return x;
    }

    callQualified() {
        printf("  %y\n", qualifiedDefault());
    }

    callUnqualified() {
        printf("  %y\n", unqualifiedDefault());
    }
}

class DefaultImpl inherits DefaultRedeclared {
    int c(int x) {
        return x * 1000;
    }
}

#! deletes the object given and returns 1: an argument that deletes the object of the call
int sub delete_arg(object o, string name) {
    printf("  arg %s\n", name);
    delete o;
    return 1;
}

#! calls whose arguments delete the object: every mode raises the error that names the class of the method
class DelBase {
    n(auto a) {
        printf("  DelBase::n() called\n");
    }

    over(auto a) {
        printf("  DelBase::over() called\n");
    }

    abstractCall() {
        abs(delete_arg(self, "abstract self call"));
    }

    abstract abs(auto a);

    selfCall() {
        n(delete_arg(self, "self call"));
    }

    selfDotCall() {
        self.n(delete_arg(self, "self.n()"));
    }
}

class DelSub inherits DelBase {
    over(auto a) {
        printf("  DelSub::over() called\n");
    }

    abs(auto a) {
        printf("  DelSub::abs() called\n");
    }

    qualifiedCall() {
        DelBase::over(delete_arg(self, "qualified self call"));
    }
}

#! issue #6057: a self call whose argument, a closure, deletes self
class SelfDelete {
    m(int a) {
        printf("  SelfDelete::m() ran a=%d\n", a);
    }

    destructor() {
        printf("  SelfDelete destructor\n");
    }

    run() {
        m((int sub () { delete self; printf("  deleted\n"); return 1; })());
        printf("  after m\n");
    }
}

#! a subclass of a builtin class: the arguments of a builtin method delete the object
class DelMutex inherits Mutex {
    int kill() {
        delete self;
        return 1;
    }

    selfCall() {
        lock(kill());
    }

    selfDotCall() {
        self.lock(kill());
    }

    deletedSelfCall() {
        delete self;
        lock();
    }
}

int sub delete_object(object o) {
    delete o;
    return 1;
}

sub check(string name, code c) {
    printf("%s:\n", name);
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        printf("  %s: %s\n", ex.err, ex.desc);
        # a call raises one exception for a deleted object, with nothing chained
        while (ex.next) {
            ex = ex.next;
            printf("  next %s: %s\n", ex.err, ex.desc);
        }
    }
}

sub cases() {
    # a private or missing method on a valid object
    check("private method", sub () { object o = new C(); o.p(arg("private")); });
    check("missing method", sub () { object o = new C(); o.nomethod(arg("missing")); });
    check("missing method on auto", sub () { auto o = new C(); o.nomethod(arg("auto")); });
    check("missing method, literal argument", sub () { object o = new C(); o.nomethod(1); });
    check("private override", sub () { Parent p = new PrivateChild(); p.m(arg("private override")); });
    # calls that are made
    check("private method from the class", sub () { C c(); c.callPrivate(); });
    check("member closure", sub () { object o = new C(); o.member_closure(arg("member closure")); });
    check("pseudo-method of an object", sub () {
        object o = new C();
        printf("  %y\n", o.hasCallableMethod(string_arg("pseudo")));
    });
    check("public override", sub () { Parent p = new Child(); p.m(arg("public override")); });
    check("overloaded method", sub () { C c(); c.m(arg("overloaded")); });
    # a deleted object with an overloaded method dispatched by name
    check("overloaded, deleted", sub () { C o(); C o2 = o; delete o; o2.m(1); });
    check("overloaded, deleted, string", sub () { C o(); C o2 = o; delete o; o2.m("x"); });
    # a qualified call through self calls its method on an object of any class
    check("qualified self call", sub () { QChild c(); c.m(1); });
    check("qualified self call, subclass", sub () { QGrandChild c(); c.m(1); });
    check("qualified self call, deleted", sub () { QChild c(); c.del(); });
    check("qualified self call, subclass, deleted", sub () { QGrandChild c(); c.del(); });
    # an abstract method is looked up by name, also with a qualified call
    check("qualified abstract call, private implementation", sub () { AbsPrivate o(); o.callQualified(); });
    check("qualified abstract call with a literal, private implementation", sub () {
        AbsPrivate o();
        o.callQualifiedLiteral();
    });
    check("abstract call, private implementation", sub () { AbsPrivate o(); o.callUnqualified(); });
    check("qualified abstract call, public implementation", sub () { AbsPublic o(); o.callQualified(); });
    check("abstract call, public implementation", sub () { AbsPublic o(); o.callUnqualified(); });
    check("redeclared abstract, no arguments", sub () { RedeclaredImpl o(); o.callNoArgs(); });
    check("redeclared abstract, arguments", sub () { RedeclaredImpl o(); o.callArgs(); });
    check("redeclared abstract, deleted", sub () { RedeclaredImpl o(); o.callDeleted(); });
    check("mixed variants, int", sub () { MixedImpl o(); o.callInt(); });
    check("mixed variants, string", sub () { MixedImpl o(); o.callString(); });
    check("redeclared abstract in a default argument", sub () { DefaultImpl o(); o.callQualified(); });
    check("abstract in a default argument, unqualified", sub () { DefaultImpl o(); o.callUnqualified(); });
    # the arguments delete the object of the call
    check("argument deletes, typed", sub () { DelBase o = new DelSub(); o.n(delete_arg(o, "typed")); });
    check("argument deletes, typed exact class", sub () { DelSub o(); o.over(delete_arg(o, "exact class")); });
    check("argument deletes, untyped", sub () { object o = new DelSub(); o.over(delete_arg(o, "untyped")); });
    check("argument deletes, inherited method", sub () { object o = new DelSub(); o.n(delete_arg(o, "inherited")); });
    check("argument deletes, self call", sub () { DelSub o(); o.selfCall(); });
    check("argument deletes, self.n()", sub () { DelSub o(); o.selfDotCall(); });
    check("argument deletes, qualified self call", sub () { DelSub o(); o.qualifiedCall(); });
    check("argument deletes, abstract self call", sub () { DelSub o(); o.abstractCall(); });
    check("argument deletes, self call with a closure argument (#6057)", sub () { (new SelfDelete()).run(); });
    check("argument deletes, builtin method", sub () { DelMutex o(); o.lock(delete_object(o)); });
    check("argument deletes, builtin method, untyped", sub () { object o = new DelMutex(); o.lock(delete_object(o)); });
    check("argument deletes, builtin self call", sub () { DelMutex o(); o.selfCall(); });
    check("argument deletes, builtin self.m()", sub () { DelMutex o(); o.selfDotCall(); });
    check("deleted object, builtin self call", sub () { DelMutex o(); o.deletedSelfCall(); });
}

# the cases run more than once, so that the tiered execution mode compiles them
for (int i = 0; i < 3; ++i) {
    printf("round %d\n", i);
    cases();
}
