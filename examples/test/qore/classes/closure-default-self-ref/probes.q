# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the probes of closure-default-self-ref.qtest (qore#6417, qore#5949): references to self and to members through self,
# assignments through self, reads of self and nested closures in the default values of closures created in methods

%modern

sub set3(reference r) {
    r = 3;
}

sub replace_obj(reference r) {
    r = new B(99);
}

string sub res(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

class B {
    public {
        int a = 1;
        int b = 10;
    }

    constructor(int a = 1) {
        self.a = a;
    }

    # a reference to a member through self, passed on to a function (qore#6417)
    code refMember() {
        return auto sub (auto x = set3(\self.a)) { return a; };
    }

    # a reference to a member through self as the default value of a reference parameter
    code refParam() {
        return auto sub (reference x = \self.a) { x = 7; return a; };
    }

    # a reference to self itself: a write through it does not replace the closure's object
    code refSelf() {
        return auto sub (auto x = replace_obj(\self)) { return a; };
    }

    # a reference to self as the default value of a reference parameter; reads through it see the object
    code refSelfParam() {
        return auto sub (reference x = \self) { return x.a; };
    }

    # reads of self and of a member through self
    code read() {
        return auto sub (auto x = self.a, auto y = self) { return sprintf("%d %s", x, y.className()); };
    }

    # an assignment through self (qore#5949)
    code assign() {
        return auto sub (auto x = (self.a = 20)) { return x + a; };
    }

    # compound assignments through self (qore#5949)
    code compound() {
        return auto sub (auto x = (self.a += 5), auto y = (self.b *= 2)) { return sprintf("%d %d %d %d", x, y, a, b); };
    }

    # a closure created by a default value, which takes a reference through self and assigns through self
    code nested() {
        return auto sub (code g = auto sub () { set3(\self.b); self.a = 4; return self.a + self.b; }) {
            return g();
        };
    }

    # the closure's code uses self as well, which it binds when it runs, after the default values
    code captured() {
        return auto sub (auto x = set3(\self.a)) { return self.a; };
    }

    # a default value that does not use self, in a static method: no object
    static code staticClosure() {
        return auto sub (auto x = 5) { return x; };
    }
}

#! the only strong reference to the object is dropped by a default value: the object stays valid while the default
#! values are evaluated, and is destroyed after them with the exception sink of the call, which its destructor's
#! exception reaches (qore#6417)
our *D gd;
string dlog;

class D {
    public {
        int n = 1;
    }

    destructor() {
        dlog += "~D";
        # an exception from the destructor reaches the caller of the closure: the destructor runs with the exception
        # sink of the evaluation of the default values
        throw "D-DESTRUCTOR", "destroyed";
    }

    code make() {
        return auto sub (auto x = (gd = NOTHING, set3(\self.n), dlog += sprintf("n=%d;", n))) { return n; };
    }
}

#! another object calls the closures: the default values use the object of the closure, not that of the caller
class C {
    public {
        int a = 100;
    }

    string call(code c) {
        return res(c);
    }
}

string sub probes() {
    list<string> l = ();
    C c();
    {
        B b();
        l += "ref member: " + c.call(b.refMember()) + " " + b.a;
    }
    {
        B b();
        l += "ref param: " + c.call(b.refParam()) + " " + b.a;
    }
    {
        B b();
        l += "ref self: " + c.call(b.refSelf()) + " " + b.a;
    }
    {
        B b(6);
        l += "ref self param: " + c.call(b.refSelfParam());
    }
    {
        B b(8);
        l += "read: " + c.call(b.read());
    }
    {
        B b();
        l += "assign: " + c.call(b.assign()) + " " + b.a;
    }
    {
        B b();
        l += "compound: " + c.call(b.compound()) + " " + b.a + " " + b.b;
    }
    {
        B b();
        l += "nested: " + c.call(b.nested()) + " " + b.a + " " + b.b;
    }
    {
        B b();
        l += "captured: " + c.call(b.captured()) + " " + b.a;
    }
    l += "static: " + res(B::staticClosure());
    {
        dlog = "";
        gd = new D();
        code dc = gd.make();
        l += "released: " + res(dc) + " " + dlog;
    }
    return foldl $1 + "\n" + $2, l;
}
