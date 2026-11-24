# -*- mode: qore; indent-tabs-mode: nil -*-
# the classes and calls of abstract-redeclared-parent-order.qtest that inherit the classes of the namespace ARO, which
# probe.q declares itself and module-user.q loads from the module; included by both

# each class inherits the abstract method from a class that declares it (C0) and from a class that redeclares it over
# an implementation (C4); the class is resolved the same way with the parent classes in either order (qore#7382)
class U4a inherits ARO::C4, ARO::C0 { string via() { return m(); } }
class U4b inherits ARO::C0, ARO::C4 { string via() { return m(); } }
class U4pa inherits ARO::C4p, ARO::C0 { string via() { return m(); } }
class U4pb inherits ARO::C0, ARO::C4p { string via() { return m(); } }
class U4ha inherits ARO::C4h, ARO::C0 { string via() { return m(); } }
class U4hb inherits ARO::C0, ARO::C4h { string via() { return m(); } }
class U4ia inherits ARO::C4i, ARO::C0 { string via() { return m(); } }
class U4ib inherits ARO::C0, ARO::C4i { string via() { return m(); } }
# the redeclaring class inherited privately
class U4ea inherits private ARO::C4, ARO::C0 { string via() { return m(); } }
class U4eb inherits ARO::C0, private ARO::C4 { string via() { return m(); } }
class U4fa inherits private:internal ARO::C4, ARO::C0 { string via() { return m(); } }
class U4fb inherits ARO::C0, private:internal ARO::C4 { string via() { return m(); } }
class U4ga inherits private:hierarchy ARO::C4, ARO::C0 { string via() { return m(); } }
class U4gb inherits ARO::C0, private:hierarchy ARO::C4 { string via() { return m(); } }
# the redeclaring class further down
class M4 inherits ARO::C4 {}
class U4ma inherits M4, ARO::C0 { string via() { return m(); } }
class U4mb inherits ARO::C0, M4 { string via() { return m(); } }
class U6a inherits ARO::C6, ARO::C0 { string via() { return m(); } }
class U6b inherits ARO::C0, ARO::C6 { string via() { return m(); } }
class U6c inherits ARO::C6, ARO::C4, ARO::C0 { string via() { return m(); } }
class U6d inherits ARO::C0, ARO::C4, ARO::C6 { string via() { return m(); } }
# the implementation inherited with private:internal access by the redeclaring class
class UXa inherits ARO::X4, ARO::X0 { string via() { return m(); } }
class UXb inherits ARO::X0, ARO::X4 { string via() { return m(); } }
# overloads
class UBa inherits ARO::B4, ARO::B0 { string via(auto a) { return m(a); } }
class UBb inherits ARO::B0, ARO::B4 { string via(auto a) { return m(a); } }
# const variants and private:internal edges
class UPa inherits ARO::P5, ARO::P1 { string via(bool b) { return m(b); } }
class UPb inherits ARO::P1, ARO::P5 { string via(bool b) { return m(b); } }
# subclasses
class W4a inherits U4a {}
class W4b inherits U4b {}
class V7 inherits ARO::N7 {}
class V8 inherits ARO::N8 {}

string sub call(object o, softlist<auto> args) {
    try {
        return call_object_method_args(o, "m", args);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub callVia(object o, softlist<auto> args) {
    try {
        return call_object_method_args(o, "via", args);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub callDirect(object o, softlist<auto> args) {
    try {
        switch (args.size()) {
            case 0: return o.m();
            default: return o.m(args[0]);
        }
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

hash<string, list<softlist<auto>>> Args = {
    "U4a": (NOTHING,), "U4b": (NOTHING,), "U4pa": (NOTHING,), "U4pb": (NOTHING,), "U4ha": (NOTHING,),
    "U4hb": (NOTHING,), "U4ia": (NOTHING,), "U4ib": (NOTHING,), "U4ea": (NOTHING,), "U4eb": (NOTHING,),
    "U4fa": (NOTHING,), "U4fb": (NOTHING,), "U4ga": (NOTHING,), "U4gb": (NOTHING,), "U4ma": (NOTHING,),
    "U4mb": (NOTHING,), "U6a": (NOTHING,), "U6b": (NOTHING,), "U6c": (NOTHING,), "U6d": (NOTHING,),
    "UXa": (NOTHING,), "UXb": (NOTHING,), "UBa": (1, "a"), "UBb": (1, "a"), "UPa": (True,), "UPb": (True,),
    "W4a": (NOTHING,), "W4b": (NOTHING,), "ARO::N7": (NOTHING,), "ARO::N8": (NOTHING,), "ARO::N9": (True,),
    "V7": (NOTHING,), "V8": (NOTHING,),
};

foreach hash<auto> i in (Args.pairIterator()) {
    object o;
    try {
        o = create_object(i.key);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", i.key, ex.err);
        continue;
    }
    list<string> l = map sprintf("m(%s): %s; direct: %s; via: %s", $1 ? $1[0].type() : "", call(o, $1),
        callDirect(o, $1), callVia(o, $1)), i.value;
    printf("%s: %s\n", i.key, l.join("; "));
}
