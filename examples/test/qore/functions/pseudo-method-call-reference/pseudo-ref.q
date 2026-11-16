# issue #995: call references to pseudo-methods (\l.size() for a list l); the output is the same in every execution
# mode and in programs compiled with qcc
%modern

class Ble {
    int huj() { return 1; }
    string name() { return "own"; }
}

#! holds a call reference to a pseudo-method of itself
class Holder {
    public {
        *code c;
        string name = "holder";
    }

    constructor(*string n) {
        if (n) {
            name = n;
        }
    }

    destructor() {
        if (name != "holder") {
            printf("destructor %s\n", name);
        }
    }
}

#! has a code member named like a pseudo-method
class Mem {
    public {
        code className = string sub () { return "member"; };
    }
}

#! makes references to pseudo-methods that depend on the class context
class K {
    private int pm() {
        return 3;
    }

    bool hcmDirect() {
        return self.hasCallableMethod("pm");
    }

    code hcmRef() {
        return \self.hasCallableMethod();
    }

    bool callInside(code c) {
        return c("pm");
    }

    bool callInside2(code c) {
        return c(method: "pm");
    }
}

#! a cycle through a reference to a pseudo-method of a hash that holds the object
sub hash_cycle() {
    Holder o("hash cycle");
    hash<auto> h = {"o": o};
    o.c = \h.size();
}

#! a cycle through a reference to a pseudo-method of a list that holds the object
sub list_cycle() {
    Holder o("list cycle");
    list<auto> l = (o,);
    o.c = \l.size();
}

sub tryit(string label, code c) {
    try {
        printf("%s: %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

Ble b();
tryit("object method", auto sub () { return (\b.huj())(); });
tryit("object pseudo-method", auto sub () { return (\b.className())(); });
tryit("object own method named like no pseudo-method", auto sub () { return (\b.name())(); });

list<int> l = (1, 2, 3);
tryit("list size", auto sub () { return (\l.size())(); });
tryit("list join with an argument", auto sub () { return (\l.join("-"))("+"); });
tryit("list first", auto sub () { return (\l.first())(); });

string s = "abcdefghijklmnopqrstuvwxyz";
tryit("long string size", auto sub () { return (\s.size())(); });
string ss = "ab";
tryit("short string size", auto sub () { return (\ss.size())(); });
tryit("string type", auto sub () { return (\ss.type())(); });

hash<auto> h = {"a": 1, "b": 2};
tryit("hash keys", auto sub () { return (\h.keys())(); });
tryit("hash hasKey with an argument", auto sub () { return (\h.hasKey())("b"); });

binary bin = <0102>;
tryit("binary size", auto sub () { return (\bin.size())(); });
int i = 42;
tryit("int typeCode", auto sub () { return (\i.typeCode())(); });
float f = 1.5;
tryit("float typeCode", auto sub () { return (\f.typeCode())(); });
date d = 2026-01-02;
tryit("date durationSeconds", auto sub () { return (\d.typeCode())(); });
*list<int> n;
tryit("nothing typeCode", auto sub () { return (\n.typeCode())(); });
code cl = int sub (int a) { return a; };
tryit("closure typeCode", auto sub () { return (\cl.typeCode())(); });

auto a = (1, 2);
tryit("auto list size", auto sub () { return (\a.size())(); });
a = "xyz";
tryit("auto string size", auto sub () { return (\a.size())(); });
tryit("auto unknown pseudo-method", auto sub () { return (\a.noSuchMethod())(); });

# the reference keeps the value that it was made from
list<int> l2 = (1, 2, 3);
code c = \l2.size();
l2 += 4;
tryit("kept value", auto sub () { return c(); });
tryit("current value", auto sub () { return (\l2.size())(); });

tryit("equal references", auto sub () { return \l.size() == \l.size(); });
tryit("references to other methods", auto sub () { return \l.size() == \l.first(); });
tryit("references to other values", auto sub () { return \l.size() == \l2.size(); });
tryit("call_function", auto sub () { return call_function(\l.size()); });
tryit("call_function_args", auto sub () { return call_function_args(\l.join(), ("/",)); });
tryit("map", auto sub () { return map (\$1.size())(), ((1,), (1, 2), "abc"); });
tryit("callref type", auto sub () { return (\l.size()).typeCode() == NT_CALLREF; });

# objects: a pseudo-method of a deleted object, as a direct call on it
Ble b2();
code c2 = \b2.className();
Ble b3 = b2;
delete b2;
tryit("deleted object, direct call", auto sub () { return b3.className(); });
tryit("deleted object, reference", auto sub () { return c2(); });
# a temporary object lives until the end of the statement that calls the reference
tryit("temporary object", auto sub () { return (\(new Ble()).className())(); });
auto ao = new Ble();
tryit("auto object pseudo-method", auto sub () { return (\ao.className())(); });
tryit("auto object method", auto sub () { return (\ao.huj())(); });
# an object can hold a reference to its own pseudo-method (the reference holds it weakly)
{
    Holder hh();
    hh.c = \hh.className();
    tryit("object holding its own reference", auto sub () { return hh.c(); });
}
# a cycle through the value of a reference is collected when it can no longer be reached
hash_cycle();
list_cycle();
printf("after the cycles\n");

# a code member with the name of the method is called, as by a direct call
hash<auto> hm = {"size": int sub () { return 99; }, "f": int sub (int x) { return x * 2; }};
tryit("hash code member, direct call", auto sub () { return hm.size(); });
tryit("hash code member, reference", auto sub () { return (\hm.size())(); });
auto ahm = hm;
tryit("hash code member with arguments, direct call", auto sub () { return ahm.f(3); });
tryit("hash code member with arguments, reference", auto sub () { return (\ahm.f())(3); });
Mem mem();
tryit("object code member, direct call", auto sub () { return mem.className(); });
tryit("object code member, reference", auto sub () { return (\mem.className())(); });

# the class context of the code that made the reference
K k();
tryit("class context, direct call", auto sub () { return k.hcmDirect(); });
tryit("class context, reference made in the class", auto sub () { return k.hcmRef()("pm"); });
tryit("class context, reference made outside the class", auto sub () { return k.callInside(\k.hasCallableMethod()); });

# a direct call of a pseudo-method on an object of an unrelated class keeps the class context of its code
class RB {
    pub() {}
}
class RA {
    private static spriv() {}

    static string run(RB b, auto ab) {
        string r = "";
        try { b.getCallReference("RA::spriv"); r += "typed ok; "; } catch (hash<ExceptionInfo> ex) { r += "typed " + ex.err + "; "; }
        try { ab.getCallReference("RA::spriv"); r += "auto ok"; } catch (hash<ExceptionInfo> ex) { r += "auto " + ex.err; }
        return r;
    }
}
RB rb();
tryit("class context of a direct call", auto sub () { return RA::run(rb, rb); });

# named arguments
tryit("named arguments to a pseudo-method of an object, direct call", auto sub () {
    return k.hasCallableMethod(method: "pm");
});
tryit("named arguments to a pseudo-method of an object, reference", auto sub () {
    return k.callInside2(\k.hasCallableMethod());
});
tryit("named arguments to a code member named like a pseudo-method, reference", auto sub () {
    return (\mem.className())(x: 1);
});
string hw = "hello world";
tryit("named arguments, direct call", auto sub () { return hw.substr(start: 6); });
tryit("named arguments, reference", auto sub () { return (\hw.substr())(start: 6); });

# the reference holds its value: a list holding an object
{
    list<auto> lo = (new Ble(), 1);
    code lc = \lo.size();
    remove lo;
    tryit("list held by the reference", auto sub () { return lc(); });
}
