#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (C) 2026 Qore Technologies, s.r.o.

# method calls on an object that the arguments of the call delete, and on an object deleted before the call: the error
# names the class of the code that the call enters (the class of the variant that it runs), as the AST interpreter
# names it (issue #5824 follow-up, issue #6168); its output is DeletedByArgsVariantClass.expected in every execution
# mode and in programs compiled with qcc

%modern

class Base {
    abstract string n(int x);
}

class Impl {
    string n(int x) {
        return "Impl::n " + x;
    }

    private string p(int x) {
        return "Impl::p " + x;
    }
}

class PBase {
    abstract string p(int x);
}

# the implementation of the abstract method is in another parent class
class D inherits Base, PBase, Impl {
    int del() {
        delete self;
        return 11;
    }

    # a self call whose argument deletes the object
    string selfArgDeletes() {
        return n(del());
    }

    # a call through a typed local whose argument deletes the object
    string dotArgDeletes() {
        D d = self;
        return d.n(del());
    }

    # a call of a private implementation
    string privateArgDeletes() {
        return p(del());
    }

    # the object is deleted before the call
    string deletedBefore() {
        delete self;
        return n(9);
    }
}

# a subclass that overrides the implementation
class E inherits D {
    string n(int x) {
        return "E::n " + x;
    }
}

# a final class, whose calls can be bound when the program is compiled
final class FD inherits Base, Impl {
    int del() {
        delete self;
        return 12;
    }

    string selfArgDeletes() {
        return n(del());
    }

    string deletedBefore() {
        delete self;
        return n(13);
    }
}

# an overloaded method with variants from more than one class
class V1 {
    string v(int x) {
        return "V1::v(int) " + x;
    }
}

class V2 inherits V1 {
    string v(string s) {
        return "V2::v(string) " + s;
    }

    int delInt() {
        delete self;
        return 14;
    }

    string delString() {
        delete self;
        return "x";
    }

    # the argument selects the variant of V1
    string inheritedVariant() {
        return v(delInt());
    }

    # the argument selects the variant of V2
    string ownVariant() {
        return v(delString());
    }

    string dotInheritedVariant() {
        V2 o = self;
        return o.v(delInt());
    }
}

# a plain method of the class of the call
class C {
    string m(int x) {
        return "C::m " + x;
    }

    int del() {
        delete self;
        return 15;
    }

    string argDeletes() {
        return m(del());
    }
}

# the implementation is private:internal, so the call from D is not bound to it, and a subclass overrides it
# (issue #6168)
class PIImpl {
    private:internal string n(int x) {
        return "PIImpl::n " + x;
    }
}

class PID inherits Base, PIImpl {
    string deletedBefore() {
        delete self;
        return n(16);
    }
}

class PIE inherits PID {
    string n(int x) {
        return "PIE::n " + x;
    }
}

# named arguments, bound by name to the variants of the method when the call is made
class NA {
    string m(int a, string b = "x") {
        return "NA::m " + a + " " + b;
    }
}

class NB inherits NA {
    string m(string a, int b = 1) {
        return "NB::m " + a + " " + b;
    }

    int del() {
        delete self;
        return 17;
    }

    # the names select the variant of NA
    string namedInherited() {
        return m(b: "s", a: del());
    }

    # the only argument is positional for either variant: the variant of NA
    string namedOnly() {
        return m(a: del());
    }

    # a named call through a typed local
    string dotNamed() {
        NB o = self;
        return o.m(b: "s", a: del());
    }

    string namedInTry() {
        try {
            return m(b: "s", a: del());
        } catch (hash<ExceptionInfo> ex) {
            return ex.err + ": " + ex.desc;
        }
    }
}

# a call in the handler of a block exit, which runs as a function of its own
class H2 inherits Base, Impl {
    int del() {
        delete self;
        return 18;
    }

    string inOnExit() {
        string r;
        {
            on_exit {
                try {
                    r = n(del());
                } catch (hash<ExceptionInfo> ex) {
                    r = ex.err + ": " + ex.desc;
                }
            }
        }
        return r;
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
    check("self call, argument deletes", string sub () { D d(); return d.selfArgDeletes(); });
    check("typed local, argument deletes", string sub () { D d(); return d.dotArgDeletes(); });
    check("private implementation, argument deletes", string sub () { D d(); return d.privateArgDeletes(); });
    check("deleted before the call", string sub () { D d(); return d.deletedBefore(); });
    check("subclass, self call, argument deletes", string sub () { E e(); return e.selfArgDeletes(); });
    check("subclass, typed local, argument deletes", string sub () { E e(); return e.dotArgDeletes(); });
    check("subclass, deleted before the call", string sub () { E e(); return e.deletedBefore(); });
    check("final class, argument deletes", string sub () { FD d(); return d.selfArgDeletes(); });
    check("final class, deleted before the call", string sub () { FD d(); return d.deletedBefore(); });
    check("top-level call, argument deletes", string sub () { D d(); return d.n(d.del()); });
    check("inherited variant, argument deletes", string sub () { V2 o(); return o.inheritedVariant(); });
    check("own variant, argument deletes", string sub () { V2 o(); return o.ownVariant(); });
    check("typed local, inherited variant, argument deletes", string sub () { V2 o(); return o.dotInheritedVariant(); });
    check("top-level call, inherited variant, argument deletes", string sub () { V2 o(); return o.v(o.delInt()); });
    check("plain method, argument deletes", string sub () { C c(); return c.argDeletes(); });
    check("named arguments, inherited variant", string sub () { NB o(); return o.namedInherited(); });
    check("named argument", string sub () { NB o(); return o.namedOnly(); });
    check("named arguments in a try block", string sub () { NB o(); return o.namedInTry(); });
    check("named arguments, typed local", string sub () { NB o(); return o.dotNamed(); });
    check("named arguments, top-level call", string sub () { NB o(); return o.m(b: "s", a: o.del()); });
    check("call in a block exit handler", string sub () { H2 h(); return h.inOnExit(); });
    check("private:internal implementation, deleted before the call", string sub () { PID d(); return d.deletedBefore(); });
    check("private:internal implementation, subclass, deleted before the call",
        string sub () { PIE e(); return e.deletedBefore(); });
}
