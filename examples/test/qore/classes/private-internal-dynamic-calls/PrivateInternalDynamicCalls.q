# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A method of a private:internal parent class that the code of a class can call directly can also be called by that
# code through call_object_method(), call_object_method_args() and a call reference from getCallReference(), with the
# same rule as the direct call (qore#6859); see private-internal-dynamic-calls.qtest
#
# Mid inherits Base with private:internal: the code of Mid can call the methods of Base on an object of Mid and of
# its subclass Sub; the code of Sub, code outside the hierarchy, the code of an unrelated class and the code of a
# look-alike of Mid (a class with the same name and declarations in another namespace; qore#7036) cannot.
# call_object_method() and call_object_method_args() still cannot call a private method, also not one of Mid in the
# code of Mid, which can call it directly and through a call reference.
#
# A call reference selects the variants of its method with the class context of the code that created it: one created
# with no class context calls only the public variants wherever it is called, and one created by the code of a class
# of a child Program keeps that context after the Program has been deleted.

%modern
%no-child-restrictions

class Base {
    string bm() {
        return "Base::bm";
    }

    string bma(string a) {
        return "Base::bma " + a;
    }

    static string sbm() {
        return "Base::sbm";
    }

    #! pushes its result to the queue, for a background call
    nothing bq(Queue q) {
        q.push("Base::bq");
    }

    #! a method with variants of different access: a call reference to it selects only the variants that the code
    #! that creates it can call, as a call of the method there does
    string mv(int i) {
        return "Base::mv(int)";
    }

    private:internal string mv(string s) {
        return "Base::mv(string)";
    }

    private:internal string pim() {
        return "Base::pim";
    }
}

#! returns the result of a call or the error that it raised
string sub tc(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! prints the results of the calls of a case
sub show(string label, hash<auto> h) {
    foreach hash<auto> i in (h.pairIterator()) {
        printf("%s: %s: %s\n", label, i.key, i.value);
    }
}

class Mid inherits private:internal Base {
    #! the calls of the code of an unrelated class called from the code of this class, which has no access of
    #! this class
    hash<auto> otherForms(object o) {
        return (new Other()).objForms(o);
    }

    #! a call reference to the private method of the class, made in its code: callable from anywhere
    code privateRef() {
        return self.getCallReference("mp");
    }

    #! a private method of the class itself
    private string mp() {
        return "Mid::mp";
    }

    #! the direct and dynamic calls on self
    hash<auto> selfForms() {
        return {
            "direct": tc(auto sub () { return self.bm(); }),
            "call_object_method own private": tc(auto sub () { return call_object_method(self, "mp"); }),
            "call_object_method_args own private": tc(auto sub () { return call_object_method_args(self, "mp"); }),
            "getCallReference own private": tc(auto sub () { return self.getCallReference("mp")(); }),
            "direct qualified": tc(auto sub () { return Base::bm(); }),
            "direct static": tc(auto sub () { return sbm(); }),
            "background qualified": tc(auto sub () { Queue q(); background Base::bq(q); return q.get(); }),
        } + objForms(self);
    }

    #! the dynamic calls on the object given
    hash<auto> objForms(object o) {
        return {
            "dynamic method call": tc(auto sub () { return o.bm(); }),
            "call_object_method": tc(auto sub () { return call_object_method(o, "bm"); }),
            "call_object_method_args": tc(auto sub () { return call_object_method_args(o, "bma", "x"); }),
            "getCallReference": tc(auto sub () { return o.getCallReference("bm")(); }),
            "getCallReference qualified": tc(auto sub () { return o.getCallReference("Base::bm")(); }),
            "getCallReference args": tc(auto sub () { return o.getCallReference("bma")("y"); }),
            "call_object_method static": tc(auto sub () { return call_object_method(o, "sbm"); }),
            "getCallReference static": tc(auto sub () { return o.getCallReference("sbm")(); }),
            "call_object_method private:internal": tc(auto sub () { return call_object_method(o, "pim"); }),
            "getCallReference private:internal": tc(auto sub () { return o.getCallReference("pim")(); }),
            "call_object_method private": tc(auto sub () { return call_object_method(o, "mp"); }),
            "getCallReference private": tc(auto sub () { return o.getCallReference("mp")(); }),
            "direct mixed public": tc(auto sub () { return o.mv(1); }),
            "direct mixed private:internal": tc(auto sub () { return o.mv("x"); }),
            "call_object_method mixed public": tc(auto sub () { return call_object_method(o, "mv", 1); }),
            "call_object_method mixed private:internal": tc(auto sub () { return call_object_method(o, "mv", "x"); }),
            "getCallReference mixed public": tc(auto sub () { return o.getCallReference("mv")(1); }),
            "getCallReference mixed private:internal": tc(auto sub () { return o.getCallReference("mv")("x"); }),
            "getCallReference qualified mixed private:internal": tc(auto sub () {
                return o.getCallReference("Base::mv")("x");
            }),
            "hasCallableMethod": tc(auto sub () { return o.hasCallableMethod("bm"); }),
            "hasCallableMethod static": tc(auto sub () { return o.hasCallableMethod("sbm"); }),
            "hasCallableMethod private:internal": tc(auto sub () { return o.hasCallableMethod("pim"); }),
            "call_object_method in method": tc(auto sub () { return comIn(o); }),
            "getCallReference in method": tc(auto sub () { return refIn(o); }),
        };
    }

    #! call_object_method() made in the method itself, not in a closure
    auto comIn(object o) {
        return call_object_method(o, "bm");
    }

    #! a call reference from getCallReference() made in the method itself, not in a closure
    auto refIn(object o) {
        return o.getCallReference("bm")();
    }
}

class Sub inherits Mid {
    #! the dynamic calls on self from the code of Sub: Sub cannot call the methods of Base (nor directly, see below)
    hash<auto> subForms() {
        return {
            "dynamic method call": tc(auto sub () { return cast<object>(self).bm(); }),
            "call_object_method": tc(auto sub () { return call_object_method(self, "bm"); }),
            "call_object_method_args": tc(auto sub () { return call_object_method_args(self, "bma", "x"); }),
            "getCallReference": tc(auto sub () { return self.getCallReference("bm")(); }),
            "getCallReference qualified": tc(auto sub () { return self.getCallReference("Base::bm")(); }),
            "call_object_method static": tc(auto sub () { return call_object_method(self, "sbm"); }),
            "getCallReference static": tc(auto sub () { return self.getCallReference("sbm")(); }),
            "hasCallableMethod": tc(auto sub () { return self.hasCallableMethod("bm"); }),
            "hasCallableMethod static": tc(auto sub () { return self.hasCallableMethod("sbm"); }),
        };
    }
}

#! a class outside the hierarchy
class Other {
    hash<auto> objForms(object o) {
        return {
            "call_object_method private": tc(auto sub () { return call_object_method(o, "mp"); }),
            "getCallReference private": tc(auto sub () { return o.getCallReference("mp")(); }),
            "dynamic method call": tc(auto sub () { return o.bm(); }),
            "call_object_method": tc(auto sub () { return call_object_method(o, "bm"); }),
            "call_object_method_args": tc(auto sub () { return call_object_method_args(o, "bma", "x"); }),
            "getCallReference": tc(auto sub () { return o.getCallReference("bm")(); }),
            "getCallReference qualified": tc(auto sub () { return o.getCallReference("Base::bm")(); }),
            "call_object_method static": tc(auto sub () { return call_object_method(o, "sbm"); }),
            "getCallReference static": tc(auto sub () { return o.getCallReference("sbm")(); }),
            "hasCallableMethod": tc(auto sub () { return o.hasCallableMethod("bm"); }),
            "hasCallableMethod static": tc(auto sub () { return o.hasCallableMethod("sbm"); }),
        };
    }
}

#! Base inherited with private (private:hierarchy): the dynamic calls follow only private:internal edges, so
#! call_object_method() and call_object_method_args() cannot call its public methods, which a direct call and a call
#! reference reach (the edge is also visible to the code of a base class, whose dispatchers can take a method name
#! from their input)
class MidH inherits private Base {
    hash<auto> hForms(object o) {
        return {
            "direct": tc(auto sub () { return o.bm(); }),
            "call_object_method": tc(auto sub () { return call_object_method(o, "bm"); }),
            "call_object_method_args": tc(auto sub () { return call_object_method_args(o, "bma", "x"); }),
            "call_object_method static": tc(auto sub () { return call_object_method(o, "sbm"); }),
            "getCallReference": tc(auto sub () { return o.getCallReference("bm")(); }),
            "getCallReference qualified": tc(auto sub () { return o.getCallReference("Base::bm")(); }),
            "call_object_method private:internal": tc(auto sub () { return call_object_method(o, "pim"); }),
            "call_object_method mixed private:internal": tc(auto sub () { return call_object_method(o, "mv", "x"); }),
            "getCallReference mixed private:internal": tc(auto sub () { return o.getCallReference("mv")("x"); }),
        };
    }
}

class SubH inherits MidH {}

namespace NS {
    #! a look-alike of ::Mid: another class with the same name and declarations
    class Mid inherits private:internal Base {
        #! the calls of the code of an unrelated class called from the code of this class, which has no access of
        #! this class
        hash<auto> otherForms(object o) {
            return (new Other()).objForms(o);
        }

        #! a call reference to the private method of the class, made in its code: callable from anywhere
        code privateRef() {
            return self.getCallReference("mp");
        }

        #! a private method of the class itself
        private string mp() {
            return "Mid::mp";
        }

        #! the direct and dynamic calls on self
        hash<auto> selfForms() {
            return {
                "direct": tc(auto sub () { return self.bm(); }),
                "call_object_method own private": tc(auto sub () { return call_object_method(self, "mp"); }),
                "call_object_method_args own private": tc(auto sub () { return call_object_method_args(self, "mp"); }),
                "getCallReference own private": tc(auto sub () { return self.getCallReference("mp")(); }),
                "direct qualified": tc(auto sub () { return Base::bm(); }),
                "direct static": tc(auto sub () { return sbm(); }),
                "background qualified": tc(auto sub () { Queue q(); background Base::bq(q); return q.get(); }),
            } + objForms(self);
        }

        #! the dynamic calls on the object given
        hash<auto> objForms(object o) {
            return {
                "dynamic method call": tc(auto sub () { return o.bm(); }),
                "call_object_method": tc(auto sub () { return call_object_method(o, "bm"); }),
                "call_object_method_args": tc(auto sub () { return call_object_method_args(o, "bma", "x"); }),
                "getCallReference": tc(auto sub () { return o.getCallReference("bm")(); }),
                "getCallReference qualified": tc(auto sub () { return o.getCallReference("Base::bm")(); }),
                "getCallReference args": tc(auto sub () { return o.getCallReference("bma")("y"); }),
                "call_object_method static": tc(auto sub () { return call_object_method(o, "sbm"); }),
                "getCallReference static": tc(auto sub () { return o.getCallReference("sbm")(); }),
                "call_object_method private:internal": tc(auto sub () { return call_object_method(o, "pim"); }),
                "getCallReference private:internal": tc(auto sub () { return o.getCallReference("pim")(); }),
                "call_object_method private": tc(auto sub () { return call_object_method(o, "mp"); }),
                "getCallReference private": tc(auto sub () { return o.getCallReference("mp")(); }),
                "direct mixed public": tc(auto sub () { return o.mv(1); }),
                "direct mixed private:internal": tc(auto sub () { return o.mv("x"); }),
                "call_object_method mixed public": tc(auto sub () { return call_object_method(o, "mv", 1); }),
                "call_object_method mixed private:internal": tc(auto sub () {
                    return call_object_method(o, "mv", "x");
                }),
                "getCallReference mixed public": tc(auto sub () { return o.getCallReference("mv")(1); }),
                "getCallReference mixed private:internal": tc(auto sub () { return o.getCallReference("mv")("x"); }),
                "getCallReference qualified mixed private:internal": tc(auto sub () {
                    return o.getCallReference("Base::mv")("x");
                }),
                "hasCallableMethod": tc(auto sub () { return o.hasCallableMethod("bm"); }),
                "hasCallableMethod static": tc(auto sub () { return o.hasCallableMethod("sbm"); }),
                "hasCallableMethod private:internal": tc(auto sub () { return o.hasCallableMethod("pim"); }),
                "call_object_method in method": tc(auto sub () { return comIn(o); }),
                "getCallReference in method": tc(auto sub () { return refIn(o); }),
            };
        }

        #! call_object_method() made in the method itself, not in a closure
        auto comIn(object o) {
            return call_object_method(o, "bm");
        }

        #! a call reference from getCallReference() made in the method itself, not in a closure
        auto refIn(object o) {
            return o.getCallReference("bm")();
        }
    }
}

Mid m();
Sub s();

# the code of Mid on an object of Mid and of Sub: every call of a method of Base is made, as the direct call, and of
# the private method of Mid
show("mid on mid", m.selfForms());
show("mid on sub", s.selfForms());
# the code of Mid on another object
show("mid on other sub", m.objForms(s));

# the code of Sub, outside the hierarchy, of an unrelated class and of a look-alike of Mid: refused
show("sub on sub", s.subForms());
show("outside", {
    "dynamic method call": tc(auto sub () { return cast<object>(s).bm(); }),
    "call_object_method": tc(auto sub () { return call_object_method(s, "bm"); }),
    "call_object_method_args": tc(auto sub () { return call_object_method_args(s, "bma", "x"); }),
    "getCallReference": tc(auto sub () { return s.getCallReference("bm")(); }),
    "getCallReference qualified": tc(auto sub () { return s.getCallReference("Base::bm")(); }),
    "call_object_method static": tc(auto sub () { return call_object_method(s, "sbm"); }),
    "getCallReference static": tc(auto sub () { return s.getCallReference("sbm")(); }),
    "hasCallableMethod": tc(auto sub () { return s.hasCallableMethod("bm"); }),
    "hasCallableMethod static": tc(auto sub () { return s.hasCallableMethod("sbm"); }),
    "call_object_method private": tc(auto sub () { return call_object_method(s, "mp"); }),
    "getCallReference private": tc(auto sub () { return s.getCallReference("mp")(); }),
});
show("other on sub", (new Other()).objForms(s));
show("other on mid", (new Other()).objForms(m));
# the code of an unrelated class called from the code of Mid has no access of Mid
show("other from mid on sub", m.otherForms(s));
show("other from mid on mid", m.otherForms(m));
NS::Mid look();
show("look-alike on sub", look.objForms(s));
show("look-alike on mid", look.objForms(m));
# the look-alike calls the methods of Base on its own objects
show("look-alike on itself", look.selfForms());

# a reference to the private method of Mid made in its code is called from outside the class
show("outside", {"private reference made in the class": tc(auto sub () { return m.privateRef()(); })});

# a call reference made outside the class to a method with variants of different access selects only the public
# variant
{
    Base b();
    show("outside on base", {
        "getCallReference mixed public": tc(auto sub () { return b.getCallReference("mv")(1); }),
        "getCallReference mixed private:internal": tc(auto sub () { return b.getCallReference("mv")("x"); }),
        "direct mixed private:internal": tc(auto sub () { return cast<object>(b).mv("x"); }),
    });
}

# the private (private:hierarchy) edge: the code of MidH calls the public methods of Base on its objects and on the
# objects of its subclass directly and through a call reference, but not with call_object_method(); code outside the
# hierarchy cannot
MidH mh();
SubH sh();
show("midh on midh", mh.hForms(mh));
show("midh on subh", mh.hForms(sh));
show("outside on subh", {
    "call_object_method": tc(auto sub () { return call_object_method(sh, "bm"); }),
    "getCallReference": tc(auto sub () { return sh.getCallReference("bm")(); }),
});

# a direct call of a method of Base in the code of Sub is refused, as the dynamic calls above are
{
    Program p(PO_NEW_STYLE);
    try {
        p.parse("class B { string bm() { return \"B::bm\"; } } class M inherits private:internal B {} "
            "class S inherits M { string f() { return self.bm(); } } string sub run() { return (new S()).f(); }",
            "sub-direct");
        printf("sub direct: %y\n", p.callFunction("run"));
    } catch (hash<ExceptionInfo> ex) {
        printf("sub direct: %s\n", ex.err);
    }
}

#! a class with methods with variants of different access, whose code calls a call reference that it is given
class CV {
    string mv(int i) {
        return "CV::mv(int)";
    }

    private string mv(string s) {
        return "CV::mv(string)";
    }

    private:internal string mv(float f) {
        return "CV::mv(float)";
    }

    static string sv(int i) {
        return "CV::sv(int)";
    }

    private static string sv(string s) {
        return "CV::sv(string)";
    }

    #! calls the call reference in the code of the class
    auto callRef(code r, auto arg) {
        return r(arg);
    }
}

class DV inherits CV {
    #! calls the call reference in the code of the subclass
    auto callRefSub(code r, auto arg) {
        return r(arg);
    }
}

# a call reference made with no class context selects only the public variants of its method, also when the code of
# the class or of its subclass calls it
{
    CV cv();
    DV dv();
    # an object of the subclass with the declared type of the class: the reference is resolved for the object's class
    CV cd = dv;
    hash<string, code> refs = {
        "getCallReference": cv.getCallReference("mv"),
        "parse-time": \cv.mv(),
        "parse-time on subclass object": \cd.mv(),
        "static getCallReference": cv.getCallReference("sv"),
        "static parse-time": \CV::sv(),
    };
    list<auto> args = (1, "x", 1.5);
    foreach hash<auto> i in (refs.pairIterator()) {
        code r = i.value;
        foreach auto arg in (args) {
            if (arg.typeCode() == NT_FLOAT && i.key =~ /^static/) {
                continue;
            }
            string label = sprintf("%s %s", i.key, arg.type());
            show("context-less reference", {
                "outside " + label: tc(auto sub () { return r(arg); }),
                "in class " + label: tc(auto sub () { return cv.callRef(r, arg); }),
                "in subclass " + label: tc(auto sub () { return dv.callRefSub(r, arg); }),
            });
        }
    }
}

public class PB {
    string m(int i) {
        return "PB::m(int)";
    }

    private string m(string s) {
        return "PB::m(string)";
    }
}

public class PBS inherits PB {}

#! returns a call reference that the code of a subclass of PB in a child Program makes, after deleting the Program
code sub child_reference(string form, PB b) {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
    p.parse("class Sub inherits PB {
    static code getRef(PB b) { return b.getCallReference(\"m\"); }
    static code parseRef(PB b) { return \\b.m(); }
}
code sub get(string form, PB b) { return form == \"getCallReference\" ? Sub::getRef(b) : Sub::parseRef(b); }
", "child");
    code r = p.callFunction("get", form, b);
    delete p;
    return r;
}

# a call reference made by the code of a class of a Program that has been deleted since is called with the access of
# its creator
{
    PB pb();
    PBS pbs();
    hash<string, code> refs = {
        "getCallReference": child_reference("getCallReference", pb),
        "parse-time": child_reference("parse-time", pb),
        "parse-time on subclass object": child_reference("parse-time", pbs),
    };
    foreach hash<auto> i in (refs.pairIterator()) {
        code r = i.value;
        show("deleted creator", {
            i.key + " int": tc(auto sub () { return r(1); }),
            i.key + " string": tc(auto sub () { return r("x"); }),
        });
    }
}
