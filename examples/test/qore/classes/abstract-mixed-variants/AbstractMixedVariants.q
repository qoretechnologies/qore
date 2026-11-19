#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (C) 2026 Qore Technologies, s.r.o.

# calls of an abstract variant that another parent class implements, when the class that declares the variant also
# has a concrete variant of the method (issue #6161); its output is AbstractMixedVariants.expected in every execution
# mode and in programs compiled with qcc

%modern

class Base {
    abstract string v(int i);
    string v(string s) {
        return "Base::v(string) " + s;
    }

    abstract string n(int x, int y);
    string n(string s) {
        return "Base::n(string) " + s;
    }

    abstract nothing put(Queue q, int i);
    nothing put(Queue q, string s) {
        q.push("Base::put(string) " + s);
    }

    # the concrete variant is private:internal
    abstract string w(int i);
    private:internal string w(string s) {
        return "Base::w(string) " + s;
    }

    string callWString() {
        return w("y");
    }

    # calls from the code of the declaring class
    string callFromBase() {
        return v(11);
    }

    string callBackgroundFromBase() {
        Queue q();
        # the thread pushes the result of the call, or its error
        background sub () {
            try {
                put(q, 12);
            } catch (hash<ExceptionInfo> ex) {
                q.push(sprintf("%s: %s", ex.err, ex.desc));
            }
        }();
        return q.get();
    }
}

class Impl {
    string v(int i) {
        return "Impl::v(int) " + i;
    }

    string n(int x, int y) {
        return sprintf("Impl::n x: %d y: %d", x, y);
    }

    nothing put(Queue q, int i) {
        q.push("Impl::put(int) " + i);
    }

    string w(int i) {
        return "Impl::w(int) " + i;
    }
}

# the abstract variants of Base are implemented by the second parent, Impl
class D inherits Base, Impl {
    string callV() {
        return v(1);
    }

    string callQualified() {
        return Base::v(2);
    }

    string callConcrete() {
        return v("x");
    }

    string callBackground() {
        Queue q();
        # the thread pushes the result of the call, or its error
        background sub () {
            try {
                put(q, 7);
            } catch (hash<ExceptionInfo> ex) {
                q.push(sprintf("%s: %s", ex.err, ex.desc));
            }
        }();
        return q.get();
    }

    string callNamed() {
        return n(y: 4, x: 3);
    }

    string callClosure() {
        code c = string sub (int i) { return v(i); };
        return c(14);
    }

    string callReference() {
        code c = \v();
        return c(15);
    }

    string callW() {
        return w(16);
    }

    # the variant is resolved at run time from the type of the argument
    string callAuto(auto a) {
        return v(a);
    }
}

# a final class, whose calls can be bound when the program is compiled
final class H inherits Base, Impl {
    string callV() {
        return v(9);
    }
}

# a subclass of D overrides the implementation
class E inherits D {
    string v(int i) {
        return "E::v(int) " + i;
    }
}

# the implementing class is the first parent
class F inherits Impl, Base {
    string callV() {
        return v(4);
    }
}

# the concrete variant is the one called; the class of the object gets the implementation of the abstract variant
# from the other parent class
class CBase {
    abstract string u(string s);
    string u(int i) {
        return "CBase::u(int) " + i;
    }

    string callU(auto a) {
        return u(a);
    }
}

class CImpl {
    string u(string s) {
        return "CImpl::u(string) " + s;
    }
}

class CD inherits CBase, CImpl {
}

sub check(string name, code c) {
    try {
        printf("%s: %s\n", name, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", name, ex.err, ex.desc);
    }
}

# the cases run more than once, so that the tiered execution mode compiles them
for (int i = 0; i < 3; ++i) {
    printf("round %d\n", i);
    check("unqualified self call", string sub () { D d(); return d.callV(); });
    check("qualified self call", string sub () { D d(); return d.callQualified(); });
    check("concrete variant, self call", string sub () { D d(); return d.callConcrete(); });
    check("call through the object", string sub () { D d(); return d.v(5); });
    check("concrete variant through the object", string sub () { D d(); return d.v("z"); });
    check("call through a typed variable", string sub () { Base b = new D(); return b.v(6); });
    check("subclass override", string sub () { E e(); return e.callV(); });
    check("subclass override through the object", string sub () { E e(); return e.v(8); });
    check("implementing class first", string sub () { F f(); return f.callV(); });
    check("final class", string sub () { H h(); return h.callV(); });
    check("background self call", string sub () { D d(); return d.callBackground(); });
    check("named arguments", string sub () { D d(); return d.callNamed(); });
    check("named arguments through the object", string sub () { D d(); return d.n(y: 6, x: 5); });
    check("call from the declaring class", string sub () { D d(); return d.callFromBase(); });
    check("call from the declaring class, subclass", string sub () { E e(); return e.callFromBase(); });
    check("background call from the declaring class", string sub () { D d(); return d.callBackgroundFromBase(); });
    check("closure", string sub () { D d(); return d.callClosure(); });
    check("call reference", string sub () { D d(); return d.callReference(); });
    check("private:internal concrete variant, abstract variant", string sub () { D d(); return d.callW(); });
    check("private:internal concrete variant, through the object", string sub () { D d(); return d.w(17); });
    check("private:internal concrete variant", string sub () { D d(); return d.callWString(); });
    check("variant resolved at run time, self call", string sub () { D d(); return d.callAuto(18); });
    check("concrete variant resolved at run time, self call", string sub () { D d(); return d.callAuto("a"); });
    check("variant resolved at run time through the object", string sub () { D d(); auto a = 19; return d.v(a); });
    check("concrete variant resolved at run time through the object",
        string sub () { D d(); auto a = "b"; return d.v(a); });
    check("declaring class's concrete variant through the object", string sub () { CD c(); return c.u(20); });
    check("implemented variant through the object", string sub () { CD c(); return c.u("c"); });
    check("declaring class's concrete variant resolved at run time",
        string sub () { CD c(); auto a = 21; return c.u(a); });
    check("declaring class's concrete variant, self call resolved at run time",
        string sub () { CD c(); return c.callU(22); });
    check("implemented variant, self call resolved at run time", string sub () { CD c(); return c.callU("d"); });
}
