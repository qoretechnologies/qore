# -*- mode: qore; indent-tabs-mode: nil -*-
# the classes and calls of abstract-module-parent-override.qtest that inherit the classes of the namespaces AMA and
# AMB, which probe.q declares itself and module-user.q loads from the modules; included by both

class Sbool { string m(bool b) { return "Sbool::m(bool)"; } }

# the class's own m(string) overrides N8's: other parent classes declare and implement m(int) (qore#7337)
class U8 inherits private AMB::N8, AMA::I2, AMA::I1, AMA::Sint {
    string m(string s) { return "U8::m(string)"; }
    string via(auto a) { return m(a); }
}
class U8f inherits AMB::N8, AMA::I2, AMA::I1, AMA::Sint { string m(string s) { return "U8f::m(string)"; } }
class U8g inherits private AMB::N8, AMA::I2, AMA::I1 { string m(string s) { return "U8g::m(string)"; } }
class U8h inherits private AMB::N8, AMA::I1, AMA::Sint { string m(string s) { return "U8h::m(string)"; } }
class U8i inherits private:internal AMB::N8, AMA::I2, AMA::I1, AMA::Sint {
    string m(string s) { return "U8i::m(string)"; }
}
class U8j inherits private:hierarchy AMB::N8, AMA::I2, AMA::I1, AMA::Sint {
    string m(string s) { return "U8j::m(string)"; }
}
class U8k inherits AMA::I2, AMA::I1, AMA::Sint, private AMB::N8 { string m(string s) { return "U8k::m(string)"; } }
class U8l inherits private AMB::N8, AMA::I2, AMA::I1, AMA::I3, AMA::Sint, Sbool {
    string m(string s) { return "U8l::m(string)"; }
}
class U8n inherits private AMB::N8, AMA::I1, AMA::I2, AMA::I3, AMA::Sint {
    string m(string s) { return "U8n::m(string)"; }
    string m(bool b) { return "U8n::m(bool)"; }
}
# without the parent classes that declare and implement m(int)
class U8b inherits AMB::N8, AMA::I2 { string m(string s) { return "U8b::m(string)"; } }
class U8c inherits private AMB::N8, AMA::I2 { string m(string s) { return "U8c::m(string)"; } }
class U8d inherits private AMB::N8 { string m(string s) { return "U8d::m(string)"; } }
class U8e inherits AMA::I2, private AMB::N8 { string m(string s) { return "U8e::m(string)"; } }
# subclasses
class V8 inherits U8 {}
class V8b inherits U8 { string m(string s) { return "V8b::m(string)"; } }
class V8c inherits U8, AMA::I3 { string m(bool b) { return "V8c::m(bool)"; } }
class V8f inherits U8f {}
class V9 inherits AMB::N9 {}
# stays abstract: m(bool) is not implemented
class W8 inherits private AMB::N8, AMA::I2, AMA::I1, AMA::I3, AMA::Sint {
    string m(string s) { return "W8::m(string)"; }
}

string sub call(object o, auto arg) {
    try {
        return exists arg ? o.m(arg) : o.m();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

foreach string cn in ("AMB::N8", "AMB::N9", "U8", "U8f", "U8g", "U8h", "U8i", "U8j", "U8k", "U8l", "U8n", "U8b",
        "U8c", "U8d", "U8e", "V8", "V8b", "V8c", "V8f", "V9", "W8") {
    object o;
    try {
        o = create_object(cn);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", cn, ex.err);
        continue;
    }
    printf("%s: %s\n", cn, (map sprintf("m(%s): %s", $1.type(), call(o, $1)), (NOTHING, 1, "a", True)).join("; "));
}

# the class's own code
{
    U8 u();
    printf("U8 via: %s; %s\n", u.via(1), u.via("a"));
    V8 v();
    printf("V8 via: %s; %s\n", v.via(1), v.via("a"));
}

# calls with the class's type, through the parent class's type, a call reference and call_object_method()
{
    U8 u();
    printf("typed U8: %s\n", u.m("a"));
    V8 v();
    printf("typed V8: %s\n", v.m("a"));
    U8f uf();
    printf("typed U8f: %s; %s\n", uf.m("a"), uf.m(1));
    AMB::N8 n = uf;
    printf("typed U8f as N8: %s\n", n.m("a"));
    code c = \uf.m();
    printf("call reference U8f: %s\n", c("a"));
    printf("call_object_method U8: %s\n", call_object_method(u, "m", "a"));
    printf("call_object_method V8b: %s\n", call_object_method(new V8b(), "m", "a"));
}
