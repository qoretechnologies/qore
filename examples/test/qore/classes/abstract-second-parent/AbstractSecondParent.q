#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (C) 2026 Qore Technologies, s.r.o.

# calls through self of abstract methods that another parent class implements (issue #3070); its output is
# AbstractSecondParent.expected in every execution mode and in programs compiled with qcc

%modern

class Base {
    abstract string m(int i);

    abstract string o(int i);
    abstract string o(string s);

    abstract nothing put(Queue q, int i);

    abstract string n(int x, int y);

    # calls from the code of the declaring class
    string callFromBase() {
        return m(11);
    }

    string callBackgroundFromBase() {
        Queue q();
        background put(q, 12);
        # the thread raises an exception and pushes nothing if the call fails
        return q.get(60s);
    }
}

class Impl {
    string m(int i) {
        return "Impl::m " + i;
    }

    string o(int i) {
        return "Impl::o(int) " + i;
    }

    string o(string s) {
        return "Impl::o(string) " + s;
    }

    nothing put(Queue q, int i) {
        q.push("Impl::put " + i);
    }

    string n(int x, int y) {
        return sprintf("Impl::n x: %d y: %d", x, y);
    }
}

# the abstract methods of Base are implemented by the second parent, Impl
class D inherits Base, Impl {
    string callM() {
        return m(1);
    }

    string callQualified() {
        return Base::m(2);
    }

    string callOverloads() {
        return o(3) + ", " + o("x");
    }

    # a background expression that calls the method
    string callBackground() {
        Queue q();
        background put(q, 7);
        # the thread raises an exception and pushes nothing if the call fails
        return q.get(60s);
    }

    # a call on the object after it has been deleted
    string callDeleted() {
        delete self;
        return m(8);
    }

    string callDeletedQualified() {
        delete self;
        return Base::m(13);
    }

    # calls with named arguments
    string callNamed() {
        return n(y: 4, x: 3);
    }

    string callNamedQualified() {
        return Base::n(y: 6, x: 5);
    }

    # a closure and a call reference
    string callClosure() {
        code c = string sub (int i) { return m(i); };
        return c(14);
    }

    string callReference() {
        code c = \m();
        return c(15);
    }
}

# the implementing class is inherited privately
class P inherits Base, private:internal Impl {
    string callM() {
        return m(16);
    }
}

# a final class, whose calls can be bound when the program is compiled
final class H inherits Base, Impl {
    string callM() {
        return m(9);
    }

    string callBackground() {
        Queue q();
        background put(q, 10);
        # the thread raises an exception and pushes nothing if the call fails
        return q.get(60s);
    }
}

# a subclass of D overrides the implementation
class E inherits D {
    string m(int i) {
        return "E::m " + i;
    }

    nothing put(Queue q, int i) {
        q.push("E::put " + i);
    }
}

# the implementing class is the first parent
class F inherits Impl, Base {
    string callM() {
        return m(4);
    }
}

class GrandImpl {
    string m(int i) {
        return "GrandImpl::m " + i;
    }

    string o(int i) {
        return "GrandImpl::o(int) " + i;
    }

    string o(string s) {
        return "GrandImpl::o(string) " + s;
    }

    nothing put(Queue q, int i) {
        q.push("GrandImpl::put " + i);
    }

    string n(int x, int y) {
        return sprintf("GrandImpl::n x: %d y: %d", x, y);
    }
}

class MidImpl inherits GrandImpl {
}

# the implementation is inherited by the second parent from its own parent
class G inherits Base, MidImpl {
    string callM() {
        return m(5);
    }
}

# the implementations are private and private:internal (issue #6158): a private:internal method can only be called
# from its own class
class PrivBase {
    abstract string m(int i);
    abstract string n(int i);
}

class PrivImpl {
    private string m(int i) {
        return "PrivImpl::m " + i;
    }

    private:internal string n(int i) {
        return "PrivImpl::n " + i;
    }
}

class PrivD inherits PrivBase, PrivImpl {
    string callM() {
        return m(17);
    }

    string callN() {
        return n(18);
    }
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
    check("unqualified self call", string sub () { D d(); return d.callM(); });
    check("qualified self call", string sub () { D d(); return d.callQualified(); });
    check("overloaded abstract variants", string sub () { D d(); return d.callOverloads(); });
    check("call through the object", string sub () { D d(); return d.m(6); });
    check("subclass override", string sub () { E e(); return e.callM(); });
    check("subclass override, qualified", string sub () { E e(); return e.callQualified(); });
    check("implementing class first", string sub () { F f(); return f.callM(); });
    check("implementation from a grandparent", string sub () { G g(); return g.callM(); });
    check("background self call", string sub () { D d(); return d.callBackground(); });
    check("background self call, subclass", string sub () { E e(); return e.callBackground(); });
    check("call on a deleted object", string sub () { D d(); return d.callDeleted(); });
    check("final class", string sub () { H h(); return h.callM(); });
    check("final class, background self call", string sub () { H h(); return h.callBackground(); });
    check("qualified call on a deleted object", string sub () { D d(); return d.callDeletedQualified(); });
    check("named arguments", string sub () { D d(); return d.callNamed(); });
    check("named arguments, qualified", string sub () { D d(); return d.callNamedQualified(); });
    check("call from the declaring class", string sub () { D d(); return d.callFromBase(); });
    check("call from the declaring class, subclass", string sub () { E e(); return e.callFromBase(); });
    check("background call from the declaring class", string sub () { D d(); return d.callBackgroundFromBase(); });
    check("closure", string sub () { D d(); return d.callClosure(); });
    check("call reference", string sub () { D d(); return d.callReference(); });
    check("private:internal implementing class", string sub () { P p(); return p.callM(); });
    check("private implementation", string sub () { PrivD d(); return d.callM(); });
    check("private:internal implementation", string sub () { PrivD d(); return d.callN(); });
}
