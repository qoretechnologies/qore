#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a call checks its target (the object of a method call or of a method call reference, or that a value has the
# method) before it evaluates its arguments, as the AST interpreter does: an argument with side effects is not
# evaluated when the check fails (issue #5784); prints what each case evaluates and raises

%modern
%allow-weak-references
%allow-opaque-references

class C {
    m(auto a) {
        printf("  C::m() called\n");
    }
}

class A {
    constructor() {
        printf("  arg evaluated\n");
    }

    destructor() {
        printf("  arg destroyed\n");
    }
}

#! a declared string that can have no value
hashdecl StringField {
    string a;
}

class StringMember {
    public {
        string m;
    }
}

class Sub inherits C {
}

class Gate {
    auto memberGate(string name) {
        printf("  memberGate %s\n", name);
        return 1;
    }
}

#! returns a value whose type is not known at parse time
auto sub untyped_value() {
    return 1;
}

int sub arg_value() {
    printf("  arg function called\n");
    return 1;
}

#! a reference parameter is read through the reference: memberGate() runs when the argument is evaluated
sub call_with_reference(reference r) {
    C o();
    C o2 = o;
    delete o;
    o2.m(r);
}

#! a class with a declared member: a reference to an undeclared member of one of its objects raises INVALID-MEMBER
#! when it is read (a class without declared members gives no value instead)
class Declared {
    public {
        auto y;
    }
}

#! a local assigned a reference holds the reference: reading it raises INVALID-MEMBER
sub local_reference() {
    object g = new Declared();
    auto x = \g.q;
    C o();
    C o2 = o;
    delete o;
    o2.m(x);
}

#! the same with a local declared as a reference
sub typed_local_reference() {
    object g = new Declared();
    reference x = \g.q;
    C o();
    C o2 = o;
    delete o;
    o2.m(x);
}

#! a closure reads a reference parameter of the function that creates it
sub captured_reference(reference r) {
    C o();
    C o2 = o;
    delete o;
    code c = sub () {
        o2.m(r);
    };
    c();
}

#! the same with a parameter that can also have no value
sub call_with_optional_reference(*reference r) {
    C o();
    C o2 = o;
    delete o;
    o2.m(r);
}

#! returns a reference to a method of an object that has been deleted
code sub deleted_method_reference() {
    C o();
    C o2 = o;
    code r = \o2.m();
    delete o;
    return r;
}

#! the call is made in a try block of its own, whose handler is in the same function as the call
sub local_try() {
    C o();
    C o2 = o;
    delete o;
    try {
        o2.m(new A());
    } catch (hash<ExceptionInfo> ex) {
        printf("  local handler: %s\n", ex.err);
    }
}

sub check(string name, code c) {
    printf("%s:\n", name);
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        printf("  %s: %s\n", ex.err, ex.desc);
    }
}

sub cases() {
    # a method call reference to an object that has been deleted
    check("call reference", sub () { C o(); code r = \o.m(); delete o; r(new A()); });
    check("call reference of object", sub () { object o = new C(); code r = \o.m(); delete o; r(new A()); });
    check("call reference, function argument", sub () { C o(); code r = \o.m(); delete o; r(arg_value()); });
    # a method call on an object deleted through another variable
    check("method call", sub () { C o(); C o2 = o; delete o; o2.m(new A()); });
    check("method call of object", sub () { object o = new C(); object o2 = o; delete o; o2.m(new A()); });
    check("method call of auto", sub () { auto o = new C(); auto o2 = o; delete o; o2.m(new A()); });
    check("method call, function argument", sub () { C o(); C o2 = o; delete o; o2.m(arg_value()); });
    # a method call on no value: the "<nothing>" pseudo-class has no such method
    check("method call on nothing", sub () { C o(); delete o; o.m(new A()); });
    # other receivers of a deleted object
    check("method call of subclass", sub () { Sub o(); C o2 = o; delete o; o2.m(new A()); });
    check("method call through weak reference", sub () { C o(); C o2 := o; delete o; o2.m(new A()); });
    check("method call through opaque reference", sub () { C o(); C o2 @= o; delete o; o2.m(new A()); });
    check("valid method call through opaque reference", sub () { C o(); C o2 @= o; o2.m(new A()); });
    # an argument read through a reference
    check("reference argument", sub () { object g = new Gate(); call_with_reference(\g.y); });
    check("optional reference argument", sub () { object g = new Gate(); call_with_optional_reference(\g.y); });
    # locals that hold a reference: reading one raises INVALID-MEMBER, which must not come before the check
    check("local assigned a reference", \local_reference());
    check("local declared as a reference", \typed_local_reference());
    check("closure reading a reference parameter", sub () {
        object g = new Declared();
        captured_reference(\g.q);
    });
    # a call reference given by a call, by an immediate closure and in a map expression
    check("returned call reference", sub () { deleted_method_reference()(new A()); });
    check("call reference from an immediate closure", sub () {
        (code sub () { return deleted_method_reference(); })()(new A());
    });
    check("call of an int in map", sub () { map $1(arg_value()), (untyped_value(),); });
    # a handler in the function that makes the call
    check("local try block", \local_try());
    # calls of a value that is not a call reference
    check("call of an int", sub () { auto r = 1; r(arg_value()); });
    check("call of no value", sub () { auto r; r(arg_value()); });

    # the same calls with arguments without side effects
    check("call reference, local argument", sub () { C o(); code r = \o.m(); int x = 1; delete o; r(x); });
    check("method call, local argument", sub () { C o(); C o2 = o; int x = 1; delete o; o2.m(x); });
    check("method call on nothing, literal argument", sub () { C o(); delete o; o.m(1); });

    # calls whose target is valid evaluate their arguments and are made
    check("valid call reference", sub () { C o(); code r = \o.m(); r(new A()); });
    check("valid method call", sub () { C o(); o.m(new A()); });
    check("existing pseudo-method", sub () { string s = "abc"; printf("  %y\n", s.substr(arg_value())); });
    # a value declared string can have no value: the "<nothing>" pseudo-class has no such method
    check("pseudo-method on an unassigned string", sub () { string s; s.substr(arg_value()); });
    check("pseudo-method on an unassigned member", sub () { StringMember o(); o.m.substr(arg_value()); });
    check("pseudo-method on an unassigned hash member", sub () {
        hash<StringField> h();
        h.a.substr(arg_value());
    });
    # the result of a call with a return type that excludes no value
    check("pseudo-method on a call result", sub () { printf("  %y\n", "ABC".lwr().substr(arg_value())); });
    check("hash member call reference", sub () {
        hash<auto> h = {"f": sub (auto a) { printf("  closure called\n"); }};
        h.f(new A());
    });
}

# the cases run more than once, so that the tiered execution mode compiles them
for (int i = 0; i < 3; ++i) {
    printf("round %d\n", i);
    cases();
}
