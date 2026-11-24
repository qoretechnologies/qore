# -*- mode: qore; indent-tabs-mode: nil -*-
# the checks of same-name-class-type-identity.qtest with the classes of the namespaces SNA, SNB and SNM, which
# probe.q declares itself and module-user.q loads from the module SameNameClasses; included by both

# SNA::Foo and SNB::Foo have the same name and the same (empty) declarations, so the same signature but for their
# namespaces; an object of one is never a value of the other's type (qore#7397)

class UC {
    string m(SNA::Foo f) { return "m(SNA::Foo)"; }
    string m(SNB::Foo f) { return "m(SNB::Foo)"; }
}

# an abstract method declared for each type by a different parent class, implemented for SNA::Foo only
class UP { abstract string m(SNA::Foo x); }
class UQ { abstract string m(SNB::Foo x); }
class UPQ inherits UP, UQ {}
class UD inherits UPQ { string m(SNA::Foo x) { return "got " + x.className(); } }

# the same with union types
class UUP { abstract string m(union<SNA::Foo, int> x); }
class UUQ { abstract string m(union<SNB::Foo, int> x); }
class UUPQ inherits UUP, UUQ {}
class UUD inherits UUPQ { string m(union<SNA::Foo, int> x) { return "got " + x.className(); } }

# run-time selection between variants for union types
class UU {
    string m(union<SNA::Foo, int> x) { return "m(union<SNA::Foo, int>)"; }
    string m(union<SNB::Foo, string> x) { return "m(union<SNB::Foo, string>)"; }
}

string sub take_a(SNA::Foo f) {
    return "take_a(" + f.className() + ")";
}

string sub take_b(SNB::Foo f) {
    return "take_b(" + f.className() + ")";
}

sub show(string label, code c) {
    try {
        printf("%s: %s\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

sub check_overloads(object c, string label) {
    show(label + " typed SNA", auto sub () { return c.m(new SNA::Foo()); });
    show(label + " typed SNB", auto sub () { return c.m(new SNB::Foo()); });
    auto a = new SNA::Foo();
    auto b = new SNB::Foo();
    show(label + " untyped SNA", auto sub () { return c.m(a); });
    show(label + " untyped SNB", auto sub () { return c.m(b); });
    code r = \c.m();
    show(label + " call ref SNA", auto sub () { return r(a); });
    show(label + " call ref SNB", auto sub () { return r(b); });
    show(label + " call_object_method SNA", auto sub () { return call_object_method(c, "m", a); });
    show(label + " call_object_method SNB", auto sub () { return call_object_method(c, "m", b); });
}

sub check_types() {
    auto a = new SNA::Foo();
    auto b = new SNB::Foo();
    show("param SNA <- SNB", auto sub () { return take_a(b); });
    show("param SNB <- SNA", auto sub () { return take_b(a); });
    show("param SNA <- SNA", auto sub () { return take_a(a); });
    show("assign SNA <- SNB", auto sub () { SNA::Foo x = b; return x.className(); });
    show("assign *SNB <- SNA", auto sub () { *SNB::Foo x = a; return x.className(); });
    show("list<SNA> <- SNB", auto sub () { list<SNA::Foo> l = (b,); return l.size(); });
    show("cast SNA <- SNB", auto sub () { return cast<SNA::Foo>(b).className(); });
    show("instanceof", auto sub () { return sprintf("%y %y", b instanceof SNA::Foo, b instanceof SNB::Foo); });
}

sub check_abstract() {
    show("abstract", auto sub () { UQ q = create_object("UD"); return q.m(new SNB::Foo()); });
    show("abstract union", auto sub () { UUQ q = create_object("UUD"); return q.m(new SNB::Foo()); });
    UU u();
    auto b = new SNB::Foo();
    show("union typed SNB", auto sub () { return u.m(new SNB::Foo()); });
    show("union untyped SNB", auto sub () { return u.m(b); });
    show("union untyped SNA", auto sub () { auto a = new SNA::Foo(); return u.m(a); });
}

# the user classes of this program (and of the module) are not inherited, so a Program can declare its own classes
# with their names
const ProgOpts = PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION
    | PO_NO_INHERIT_USER_CLASSES;

#! a new Program with the given source
Program sub new_program(string src, string label) {
    Program p(ProgOpts);
    p.parse(src, label);
    return p;
}

sub check_programs() {
    # the same declaration in two Programs: the same type (the class signature)
    string src = "namespace XA { class Foo {} } XA::Foo sub mk() { return new XA::Foo(); } "
        "string sub take(XA::Foo f) { return 'take(XA::Foo)'; }";
    Program p1 = new_program(src, "p1");
    Program p2 = new_program(src, "p2");
    show("program same declaration", auto sub () { return p2.callFunction("take", p1.callFunction("mk")); });
    # the same class name and declarations in another namespace: another type
    Program p3 = new_program("namespace XB { class Foo {} } string sub take(XB::Foo f) { return 'take(XB::Foo)'; }",
        "p3");
    show("program other namespace", auto sub () { return p3.callFunction("take", p1.callFunction("mk")); });
    # the same in a nested namespace
    Program p4 = new_program("namespace XA { namespace XA { class Foo {} } } "
        "string sub take(XA::XA::Foo f) { return 'take(XA::XA::Foo)'; }", "p4");
    show("program nested namespace", auto sub () { return p4.callFunction("take", p1.callFunction("mk")); });
    # a class declared with a scoped name is in the namespace of its path
    string scoped = "namespace XA {} class XA::Foo {} XA::Foo sub mk() { return new XA::Foo(); } "
        "string sub take(XA::Foo f) { return 'take(XA::Foo)'; }";
    Program p5(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_NO_INHERIT_USER_CLASSES);
    p5.parse(scoped, "p5");
    show("program scoped declaration", auto sub () { return p5.callFunction("take", p1.callFunction("mk")); });
    show("program scoped declaration back", auto sub () { return p2.callFunction("take", p5.callFunction("mk")); });
    Program p6(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_NO_INHERIT_USER_CLASSES);
    p6.parse("namespace XB {} class XB::Foo {} string sub take(XB::Foo f) { return 'take(XB::Foo)'; }", "p6");
    show("program scoped other namespace", auto sub () { return p6.callFunction("take", p5.callFunction("mk")); });
    # a class of this program's namespaces passed to the overloads of another Program
    Program p7 = new_program("namespace SNA { class Foo {} } namespace SNB { class Foo {} } "
        "string sub m(SNA::Foo f) { return 'm(SNA::Foo)'; } string sub m(SNB::Foo f) { return 'm(SNB::Foo)'; }", "p7");
    show("program overloads SNA", auto sub () { return p7.callFunction("m", new SNA::Foo()); });
    show("program overloads SNB", auto sub () { return p7.callFunction("m", new SNB::Foo()); });
    # the parse-time check of a call with an object of the other class
    show("parse type error", auto sub () {
        new_program("namespace NA { class Foo {} } namespace NB { class Foo {} } sub f(NA::Foo x) {} "
            "sub g() { f(new NB::Foo()); }", "p8");
        return "no error";
    });
}

check_overloads(new UC(), "program");
check_overloads(new SNM::C(), "SNM");
check_types();
check_abstract();
check_programs();
