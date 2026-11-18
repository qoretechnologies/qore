%modern
%disable-all-warnings

# a call through a member that holds code (a closure or a call reference) applies the access rules of a read of the
# member (qore#6487)

int sub helper() {
    return 7;
}

class Base {
    public {
        code pub = int sub () { return 1; };
    }
    private {
        code priv = int sub () { return 2; };
        code privref = \helper();
    }
    private:internal {
        code intern = int sub () { return 3; };
    }

    # the class itself, on itself and on another object of the class
    auto own() {
        return (self.pub(), self.priv(), self.privref(), self.intern());
    }
    auto other(Base o) {
        return (o.pub(), o.priv(), o.privref(), o.intern());
    }
    # a closure created inside the class calls through the members
    code makeClosure() {
        return auto sub () { return (self.priv(), self.privref(), self.intern()); };
    }
    # a call reference to the member, created inside the class
    code makeRef() {
        return \self.priv();
    }
    # a call by a name given at run time, inside the class
    auto byName(string name) {
        return call_object_method(self, name);
    }
}

class Child inherits Base {
    # a private member of the parent is accessible in a child, a private:internal one is not
    auto childPriv() {
        return (self.priv(), self.privref());
    }
    auto childIntern() {
        return self.intern();
    }
}

# an unrelated class: no access to the members of Base
class Stranger {
    auto call(Base o) {
        return o.priv();
    }
    auto callIntern(Base o) {
        return o.intern();
    }
    auto callByName(Base o) {
        return call_object_method(o, "priv");
    }
}

# the result of a call or the error code, with the description's start
string sub r(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        return "! " + ex.err;
    }
}

auto b = new Base();
auto c = new Child();
Stranger s();

printf("public, outside: %s\n", r(auto sub () { return b.pub(); }));
printf("private, outside: %s\n", r(auto sub () { return b.priv(); }));
printf("private ref, outside: %s\n", r(auto sub () { return b.privref(); }));
printf("internal, outside: %s\n", r(auto sub () { return b.intern(); }));
printf("private, outside, by name: %s\n", r(auto sub () { return call_object_method(b, "priv"); }));
printf("private, outside, by name with args: %s\n",
    r(auto sub () { return call_object_method_args(b, "priv", ()); }));
printf("private, outside, call reference: %s\n", r(auto sub () { code f = \b.priv(); return f(); }));
printf("private, outside, child object: %s\n", r(auto sub () { return c.priv(); }));
printf("internal, outside, child object: %s\n", r(auto sub () { return c.intern(); }));
printf("own: %s\n", r(auto sub () { return b.own(); }));
printf("other object of the class: %s\n", r(auto sub () { return b.other(new Base()); }));
printf("closure made inside: %s\n", r(auto sub () { return b.makeClosure()(); }));
printf("reference made inside: %s\n", r(auto sub () { return b.makeRef()(); }));
printf("by name inside: %s\n", r(auto sub () { return b.byName("priv"); }));
printf("child, private: %s\n", r(auto sub () { return c.childPriv(); }));
printf("child, internal: %s\n", r(auto sub () { return c.childIntern(); }));
printf("child, other: %s\n", r(auto sub () { return c.other(b); }));
printf("stranger, private: %s\n", r(auto sub () { return s.call(b); }));
printf("stranger, internal: %s\n", r(auto sub () { return s.callIntern(b); }));
printf("stranger, by name: %s\n", r(auto sub () { return s.callByName(b); }));
