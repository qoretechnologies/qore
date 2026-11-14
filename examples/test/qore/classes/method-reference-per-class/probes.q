# probes of method-reference-per-class.qtest (qore#6062): a method call reference \o.m() whose object expression
# has no class type resolves the method for the class of each object when the reference is created, whatever the
# objects that the expression was evaluated with before; probes() returns one line per pass, so that the code runs on
# every tier with tiered execution

%modern

class HasFoo {
    string foo() { return "HasFoo::foo"; }
}

class OtherFoo {
    string foo() { return "OtherFoo::foo"; }
}

class GateFoo {
    string methodGate(string m) { return "GateFoo::methodGate(" + m + ")"; }
}

class NoFoo {
}

class ChildFoo inherits HasFoo {
}

class OverFoo inherits HasFoo {
    string foo() { return "OverFoo::foo"; }
}

class PrivOverFoo inherits HasFoo {
    private string foo() { return "PrivOverFoo::foo"; }
}

class StaticFoo {
    static string foo() { return "StaticFoo::foo"; }
}

class PrivFoo {
    private string foo() { return "PrivFoo::foo"; }

    code inside(object o) { return \o.foo(); }
}

#! one reference expression for the objects of every class
code sub foo_ref(object o) {
    return \o.foo();
}

#! a reference expression whose object has a class type: the method is resolved when parsing
code sub typed_ref(HasFoo o) {
    return \o.foo();
}

string sub ref_result(code refgen, object o) {
    try {
        code r = refgen(o);
        return r();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub objects_line(code refgen, list<object> objects) {
    list<string> l = map ref_result(refgen, $1), objects;
    return l.join(",");
}

#! a reference expression evaluated only with objects of classes of Programs that are deleted after each use
code sub deleted_class_ref(object o) {
    return \o.foo();
}

#! classes defined in Programs that are deleted after each use: a class can be given the address of a deleted one
string sub deleted_classes(int count) {
    int ok = 0;
    for (int i = 0; i < count; ++i) {
        Program p(PO_NEW_STYLE);
        p.parse(sprintf("class X { string foo() { return \"X%d\"; } } object sub make() { return new X(); }", i),
            "deleted-class");
        {
            object o = p.callFunction("make");
            code r = deleted_class_ref(o);
            if (r() == sprintf("X%d", i)) {
                ++ok;
            }
        }
        delete p;
    }
    return sprintf("%d/%d", ok, count);
}

string sub probes() {
    list<string> parts;
    # each node is evaluated first with an object of a class with the method, then with the other classes, and in
    # the other order
    parts += objects_line(\foo_ref(), (new HasFoo(), new OtherFoo(), new GateFoo(), new NoFoo()));
    parts += objects_line(code sub (object o) { return \o.foo(); },
        (new GateFoo(), new NoFoo(), new HasFoo(), new OtherFoo()));
    # alternating classes, with subclasses that inherit and override the method, and a static method
    parts += objects_line(\foo_ref(), (new OtherFoo(), new HasFoo(), new ChildFoo(), new OverFoo(), new StaticFoo(),
        new HasFoo(), new OtherFoo(), new StaticFoo()));
    # a subclass of the declared class: its own method, which may not be accessible here
    parts += objects_line(\typed_ref(), (new ChildFoo(), new OverFoo(), new HasFoo(), new PrivOverFoo(),
        new OverFoo()));
    # a private method: from outside the class, and from inside it (the result is not cached for either)
    PrivFoo pf();
    parts += objects_line(\foo_ref(), (pf, new HasFoo(), pf));
    parts += objects_line(\pf.inside(), (pf, new HasFoo(), pf));
    parts += deleted_classes(20);
    return parts.join("; ");
}
