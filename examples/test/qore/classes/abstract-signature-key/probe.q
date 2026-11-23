#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# the classes of the AbstractSignatureKey module and of module-user.q in one program, and classes of the same name in
# different namespaces; abstract-signature-key.qtest runs it in every execution mode and as qcc executables and
# compares the output with probe.expected

%modern

namespace ASK {
hashdecl H {
    int a = 1;
}

typedef IntList = list<int>;

class Foo {
    string name() {
        return "Foo";
    }
}

# one abstract declaration of each type; the class of ILater is declared after it
class IInt { abstract string m(int i); }
class IOptStr { abstract string m(*string s); }
class IListInt { abstract string m(list<int> l); }
class ITypedef { abstract string m(IntList l); }
class IHashSI { abstract string m(hash<string, int> h); }
class IObjFoo { abstract string m(object<Foo> o); }
class IFoo { abstract string m(Foo f); }
class IHashDecl { abstract string m(hash<H> h); }
class ILater { abstract string m(Later l); }
class ISoftList { abstract string m(softlist<string> l); }
class ICode { abstract string m(code c); }
class ITwo { abstract string m(int i, *string s); }
class IBox<T> { abstract string m(T t); }

class Later {
    string name() {
        return "Later";
    }
}

# two abstract parent classes with the same declaration, implemented by the class itself
class I1 { abstract string m(int i); }
class I2 { abstract string m(int i); }
class M inherits I1, I2 {
    string m(int i) {
        return "M::m(int)";
    }
}
}

# the same signatures with other parameter names, and with other names of the same types
class JInt { abstract string m(int x); }
class JOptStr { abstract string m(*string x); }
class JListInt { abstract string m(ASK::IntList x); }
class JTypedef { abstract string m(list<int> x); }
class JHashSI { abstract string m(hash<string, int> x); }
class JObjFoo { abstract string m(object<ASK::Foo> x); }
class JFoo { abstract string m(ASK::Foo x); }
class JHashDecl { abstract string m(hash<ASK::H> x); }
class JLater { abstract string m(ASK::Later x); }
class JSoftList { abstract string m(softlist<string> x); }
class JCode { abstract string m(code x); }
class JTwo { abstract string m(int x, *string y); }

# other signatures
class DStr { abstract string m(string s); }
class DReqStr { abstract string m(string s); }
class DListStr { abstract string m(list<string> l); }
class DHash { abstract string m(hash<auto> h); }
class DOne { abstract string m(int i); }

# each class inheriting both has one abstract variant
class VInt inherits ASK::IInt, JInt {}
class VOptStr inherits JOptStr, ASK::IOptStr {}
class VListInt inherits ASK::IListInt, JListInt {}
class VTypedef inherits ASK::ITypedef, JTypedef {}
class VHashSI inherits JHashSI, ASK::IHashSI {}
class VObjFoo inherits ASK::IObjFoo, JObjFoo {}
class VFoo inherits ASK::IFoo, JFoo {}
class VHashDecl inherits ASK::IHashDecl, JHashDecl {}
class VLater inherits JLater, ASK::ILater {}
class VSoftList inherits ASK::ISoftList, JSoftList {}
class VCode inherits ASK::ICode, JCode {}
class VTwo inherits ASK::ITwo, JTwo {}
# the list<int> declarations under both names
class VLists inherits ASK::IListInt, ASK::ITypedef, JListInt, JTypedef {}

# and two with different signatures
class XStr inherits ASK::IInt, DStr {}
class XReqStr inherits ASK::IOptStr, DReqStr {}
class XListStr inherits ASK::IListInt, DListStr {}
class XHash inherits ASK::IHashSI, DHash {}
class XTwo inherits ASK::ITwo, DOne {}

# a method of the class implements the declarations of both
class WInt inherits ASK::IInt, JInt { string m(int v) { return "WInt::m(int)"; } }
class WOptStr inherits ASK::IOptStr, JOptStr { string m(*string v) { return "WOptStr::m(*string)"; } }
class WListInt inherits ASK::IListInt, JListInt { string m(list<int> v) { return "WListInt::m(list<int>)"; } }
class WTypedef inherits ASK::ITypedef, JTypedef { string m(ASK::IntList v) { return "WTypedef::m(IntList)"; } }
class WHashSI inherits ASK::IHashSI, JHashSI { string m(hash<string, int> v) { return "WHashSI::m(hash)"; } }
class WObjFoo inherits ASK::IObjFoo, JObjFoo { string m(object<ASK::Foo> v) { return "WObjFoo::m(Foo)"; } }
class WFoo inherits ASK::IFoo, JFoo { string m(ASK::Foo v) { return "WFoo::m(Foo)"; } }
class WHashDecl inherits ASK::IHashDecl, JHashDecl { string m(hash<ASK::H> v) { return "WHashDecl::m(hash<H>)"; } }
class WLater inherits ASK::ILater, JLater { string m(ASK::Later v) { return "WLater::m(Later)"; } }
class WSoftList inherits ASK::ISoftList, JSoftList { string m(softlist<string> v) { return "WSoftList::m"; } }
class WCode inherits ASK::ICode, JCode { string m(code v) { return "WCode::m(code)"; } }
class WTwo inherits ASK::ITwo, JTwo { string m(int v, *string w) { return "WTwo::m(int, *string)"; } }
class WBox inherits ASK::IBox<int>, JInt { string m(int v) { return "WBox::m(int)"; } }

# the access of a privately inherited implementation is that of the declarations, whose classes are reached publicly
# or privately (qore#7288)
class K { string m(int i) { return "K::m(int)"; } }
class SStr { string m(string s) { return "SStr::m(string)"; } }
class U1 inherits private JInt, ASK::I1, private K, SStr {}
class U2 inherits ASK::I1, private JInt, private K, SStr {}
class U3 inherits private ASK::M, JInt, SStr {}
class U4 inherits JInt, private ASK::M, SStr {}
class U5 inherits private JInt, ASK::I1, private ASK::M, SStr {}
class U6 inherits private ASK::I1, JInt, private K, SStr {}
class U7 inherits private ASK::I1, private JInt, private K, SStr {}

hash<string, auto> args = {
    "Int": 1,
    "OptStr": "s",
    "ListInt": (1, 2),
    "Typedef": (1, 2),
    "HashSI": {"a": 1},
    "ObjFoo": new ASK::Foo(),
    "Foo": new ASK::Foo(),
    "HashDecl": hash<ASK::H>{},
    "Later": new ASK::Later(),
    "SoftList": "s",
    "Code": sub () {},
    "Two": 1,
    "Box": 1,
};

foreach string c in (("VInt", "VOptStr", "VListInt", "VTypedef", "VHashSI", "VObjFoo", "VFoo", "VHashDecl", "VLater",
        "VSoftList", "VCode", "VTwo", "VLists", "XStr", "XReqStr", "XListStr", "XHash", "XTwo")) {
    try {
        create_object(c);
        printf("%s: instantiated\n", c);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", c, ex.err, ex.desc);
    }
}

foreach hash<auto> i in (args.pairIterator()) {
    string c = "W" + i.key;
    try {
        object o = create_object(c);
        printf("%s: %s\n", c, o.m(i.value));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", c, ex.err, ex.desc);
    }
}

foreach string c in (("U1", "U2", "U3", "U4", "U5", "U6", "U7")) {
    try {
        object o = create_object(c);
        printf("%s: m(1) from outside: %s\n", c, o.m(1));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: m(1) from outside: %s: %s\n", c, ex.err, ex.desc);
    }
}

# declarations of different classes of the same name are different abstract variants
namespace NA {
    class Foo { public { string a = "a"; } }
    class I { abstract string m(Foo f); }
    class S { string m(Foo f) { return "NA::S::m(NA::Foo)"; } }
}
namespace NB {
    class Foo { public { int b = 1; } }
    class I { abstract string m(Foo f); }
}
class YAbs inherits NA::I, NB::I {}
# a parent class implements NA::I's declaration only
class YPar inherits NA::S, NA::I, NB::I {}
class YPar2 inherits NA::I, NB::I, NA::S {}
# the class's own method implements NA::I's declaration only
class YOwn inherits NA::I, NB::I {
    string m(NA::Foo f) {
        return "YOwn::m(NA::Foo)";
    }
}
class YBoth inherits NA::I, NB::I, NA::S {
    string m(NB::Foo f) {
        return "YBoth::m(NB::Foo)";
    }
}
foreach string c in (("YAbs", "YPar", "YPar2", "YOwn")) {
    try {
        create_object(c);
        printf("%s: instantiated\n", c);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", c, ex.err, ex.desc);
    }
}
{
    YBoth y();
    printf("YBoth: %s %s\n", y.m(new NA::Foo()), y.m(new NB::Foo()));
}

# a subclass that a default argument of a parent class's method initializes while the parent class is initialized
# gets the parent class's declarations before their keys are made from the resolved types
namespace RK {
    class LongParameterClassName {}
}
class RA {
    abstract string m(RK::LongParameterClassName x);
    abstract string n(RK::LongParameterClassName x);
    static f(*string o = RB::name(), object p = new RC()) {}
}
class RJ {
    # the same parameter names: which of the declarations RB keeps depends on the order of the initialization
    abstract string m(LongParameterClassName x);
    abstract string n(LongParameterClassName x);
}
class RB inherits RA, RJ {
    static *string name() {
    }
    string n(RK::LongParameterClassName z) {
        return "RB::n()";
    }
}
class RC inherits RA, RJ {
    static *string name() {
    }
    string m(RK::LongParameterClassName z) {
        return "RC::m()";
    }
    string n(RK::LongParameterClassName z) {
        return "RC::n()";
    }
}
try {
    create_object("RB");
    printf("RB: instantiated\n");
} catch (hash<ExceptionInfo> ex) {
    printf("RB: %s: %s\n", ex.err, ex.desc);
}
{
    RC rc();
    printf("RC: %s %s\n", rc.m(new RK::LongParameterClassName()), rc.n(new RK::LongParameterClassName()));
}
# the same with an abstract subclass, whose object cannot be created when the default argument is parsed
{
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
    try {
        p.parse("namespace NS { class LongParameterClassName {} }
class A {
    abstract string m(NS::LongParameterClassName x);
    static f(object o = new B()) {}
}
class J { abstract string m(LongParameterClassName y); }
class B inherits A, J {}", "nested");
        printf("nested: parsed\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("nested: %s: %s\n", ex.err, ex.desc);
    }
}
# a subclass whose initialization a static method call in a default argument of a parent class's method completes
# before the parent class makes its keys from the resolved types does not implement the parent class's declaration
{
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
    p.parse("namespace NS { class LongParameterClassName {} }
class A {
    abstract string m(NS::LongParameterClassName x);
    static f(int i = B::g()) {}
}
class B inherits A {
    static int g() { return 1; }
}
string sub t() {
    try {
        object o = create_object('B');
        return sprintf('instantiated: %y', o.m(new NS::LongParameterClassName()));
    } catch (hash<ExceptionInfo> ex) {
        return sprintf('%s: %s', ex.err, ex.desc);
    }
}", "nested2");
    printf("nested2: %s\n", p.callFunction("t"));
}
