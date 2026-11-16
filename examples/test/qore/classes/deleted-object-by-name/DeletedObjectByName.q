#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (C) 2026 Qore Technologies, s.r.o.

# self calls on an object deleted before the call that are looked up by name on the class of the object: the error
# names the class of the object, as the AST interpreter names it (issue #6168); its output is
# DeletedObjectByName.expected in every execution mode and in programs compiled with qcc

%modern

class X {
    string m(int i) {
        return "X::m " + i;
    }

    string deletedBefore() {
        delete self;
        return m(1);
    }
}

# overrides the method
class Y inherits X {
    string m(int i) {
        return "Y::m " + i;
    }
}

# inherits the method
class Y2 inherits X {
}

class Base {
    abstract string n(int x);
}

# the implementation of the abstract method is private:internal, so the call from PID is not bound to it
class PIImpl {
    private:internal string n(int x) {
        return "PIImpl::n " + x;
    }
}

class PID inherits Base, PIImpl {
    string deletedBefore() {
        delete self;
        return n(2);
    }
}

class PIE inherits PID {
    string n(int x) {
        return "PIE::n " + x;
    }
}

class PIE2 inherits PID {
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
    check("class of the call", string sub () { X x(); return x.deletedBefore(); });
    check("subclass with an override", string sub () { Y y(); return y.deletedBefore(); });
    check("subclass without an override", string sub () { Y2 y(); return y.deletedBefore(); });
    check("abstract, class of the call", string sub () { PID d(); return d.deletedBefore(); });
    check("abstract, subclass with an override", string sub () { PIE e(); return e.deletedBefore(); });
    check("abstract, subclass without an override", string sub () { PIE2 e(); return e.deletedBefore(); });
}
