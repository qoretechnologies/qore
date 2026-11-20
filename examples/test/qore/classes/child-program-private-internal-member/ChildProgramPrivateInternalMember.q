# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A class that inherits a parent class with private:internal is imported into a child Program; an object of a
# subclass declared in the child, and of the imported class itself, has the parent class's member, which the
# imported class's methods read and write through self and through another reference (qore#6671); see
# child-program-private-internal-member.qtest

%modern
%no-child-restrictions

class Base {
    public {
        string b = "base-member";
    }

    string bread() { return sprintf("%y", b); }
}

class Mid inherits private:internal Base {
    private {
        string p = "mid-private";
    }
    private:internal {
        string pin = "mid-internal";
    }

    string read() { return sprintf("%y", b); }
    string write(string v) { b = v; return sprintf("%y", b); }
    string readRef(object o) { return sprintf("%y", o.b); }
    string writeRef(object o, string v) { o.b = v; return sprintf("%y", o.b); }
    string baseRead() { return bread(); }
    string priv() { return sprintf("%y", p); }
    string privRef(object o) { return sprintf("%y", o.p); }
    string internal() { return sprintf("%y", pin); }
    string internalWrite(string v) { pin = v; return sprintf("%y", pin); }
}

class Sub1 inherits Mid {}

# the result of one access, or its exception
string sub try_get(code c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# the accesses of objects made by new_obj
string sub checks(code new_obj) {
    list<string> l = ();
    l += try_get(string sub () { return new_obj().read(); });
    object o = new_obj();
    l += try_get(string sub () { return o.write("w1"); });
    l += try_get(string sub () { return o.read(); });
    l += try_get(string sub () { return o.baseRead(); });
    l += try_get(string sub () { return new_obj().readRef(new_obj()); });
    object o2 = new_obj();
    l += try_get(string sub () { return o2.writeRef(o2, "w2"); });
    l += try_get(string sub () { return o2.read(); });
    l += try_get(string sub () { return new_obj().priv(); });
    l += try_get(string sub () { return new_obj().privRef(new_obj()); });
    l += try_get(string sub () { return new_obj().internal(); });
    object o3 = new_obj();
    l += try_get(string sub () { return o3.internalWrite("w3"); });
    l += try_get(string sub () { return o3.internal(); });
    return l.join(" ");
}

printf("one Program, subclass: %s\n", checks(object sub () { return new Sub1(); }));

Program p(PO_NEW_STYLE | PO_NO_INHERIT_USER_CLASSES);
p.importClass("Mid");
p.parse("class Sub inherits Mid {}
object sub new_sub() { return new Sub(); }
object sub new_mid() { return new Mid(); }
", "child");
printf("child, subclass: %s\n", checks(object sub () { return p.callFunction("new_sub"); }));
printf("child, imported class: %s\n", checks(object sub () { return p.callFunction("new_mid"); }));
