#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# the member enumerations of objects from code inside and outside of their class hierarchy (qore#7048)
# Copyright (C) 2026 Qore Technologies, s.r.o.

%modern

#! formats the results that the caller computed in its own code, whose access to the members they depend on
sub fmt(string label, list<auto> k, int s, bool e, bool v, int el, list<auto> ko) {
    # the names in their order, which is that of the keys operator
    printf("%s: keys(): %y size(): %d empty(): %y val(): %y elements: %d keys operator: %y same: %y\n", label, k, s, e,
        v, el, ko, k == ko && s == ko.size() && el == s && e == !s);
}

#! Compares the snapshots constructed in the calling scope with its keys and member values.
sub checkSnapshots(list<auto> names, hash<auto> members, AbstractIterator values, AbstractIterator keys_it,
        AbstractIterator pairs, AbstractIterator backwards, AbstractIterator reverse_keys, AbstractIterator reverse_pairs,
        AbstractIterator expected_pairs) {
    list<auto> seen = ();
    while (expected_pairs.next()) {
        hash<auto> pair = expected_pairs.getValue();
        if (!values.next() || !keys_it.next() || !pairs.next() || !members.hasKey(pair.key)
                || members{pair.key} !== pair.value || values.getValue() !== pair.value
                || keys_it.getValue() !== pair.key || pairs.getValue() !== pair) {
            throw "SNAPSHOT-ERROR", sprintf("expected %y in %y", pair, members);
        }
        seen += (pair.key,);
    }
    if (seen != names || (keys members) != names || values.next() || keys_it.next() || pairs.next()) {
        throw "SNAPSHOT-ERROR", sprintf("expected keys %y in %y", names, members);
    }
    for (int i = names.size() - 1; i >= 0; --i) {
        if (!backwards.next() || backwards.getValue() !== members{names[i]} || !reverse_keys.next()
                || reverse_keys.getValue() !== names[i] || !reverse_pairs.next()
                || reverse_pairs.getValue() !== {"key": names[i], "value": members{names[i]}}) {
            throw "SNAPSHOT-ERROR", "unexpected reverse iterator value";
        }
    }
    if (backwards.next() || reverse_keys.next() || reverse_pairs.next()) { throw "SNAPSHOT-ERROR", "extra reverse iterator value"; }
}

class Base {
    public {
        int pub_base = 1;
    }
    private {
        int priv_base = 2;
    }
    private:internal {
        int int_base = 3;
    }

    inside() {
        checkSnapshots(keys self, hash(self), new ObjectIterator(self), new ObjectKeyIterator(self),
            new ObjectPairIterator(self), new ObjectReverseIterator(self), new ObjectKeyReverseIterator(self),
            new ObjectPairReverseIterator(self), self.pairIterator());
        fmt("Base method", self.keys(), self.size(), self.empty(), self.val(), elements self, keys self);
    }
}

class Child inherits Base {
    public {
        int pub_child = 4;
    }
    private {
        int priv_child = 5;
    }

    insideChild() {
        checkSnapshots(keys self, hash(self), new ObjectIterator(self), new ObjectKeyIterator(self),
            new ObjectPairIterator(self), new ObjectReverseIterator(self), new ObjectKeyReverseIterator(self),
            new ObjectPairReverseIterator(self), self.pairIterator());
        fmt("Child method", self.keys(), self.size(), self.empty(), self.val(), elements self, keys self);
    }
}

class OnlyPrivate {
    private {
        int p = 1;
    }

    inside() {
        fmt("OnlyPrivate method", self.keys(), self.size(), self.empty(), self.val(), elements self, keys self);
    }
}

#! the public members of a privately inherited class are not public for code outside the hierarchy
class PrivateChild inherits private Base {
    public {
        int pub_pchild = 6;
    }

    insidePrivateChild() {
        fmt("PrivateChild method", self.keys(), self.size(), self.empty(), self.val(), elements self, keys self);
    }
}

#! a class without non-public members: its objects are enumerated without checking each member
class AllPublic {
    public {
        int x = 1;
        int y = 2;
    }
}

#! a class without member declarations, whose objects get members at run time
class Dynamic {
}

#! a parent class's method enumerates an object whose class has only public members
class EnumParent {
    enumerate(string label) {
        fmt("EnumParent method on " + label, self.keys(), self.size(), self.empty(), self.val(), elements self,
            keys self);
    }
}
class EnumPublicChild inherits EnumParent {
    public {
        int c1 = 1;
        int c2 = 2;
    }
}

#! a public member of the child with the name of the parent's private:internal member
class CollideParent {
    private:internal {
        int x = 1;
    }
    public {
        int y = 2;
    }
    enumerate(string label) {
        fmt("CollideParent method on " + label, self.keys(), self.size(), self.empty(), self.val(), elements self,
            keys self);
    }
}
class CollideChild inherits CollideParent {
    public {
        int x = 3;
    }
    enumerateChild() {
        fmt("CollideChild method", self.keys(), self.size(), self.empty(), self.val(), elements self, keys self);
    }
}

#! private:internal inheritance
class InternalChild inherits private:internal Base {
    public {
        int pub_ichild = 7;
    }
    insideInternalChild() {
        fmt("InternalChild method", self.keys(), self.size(), self.empty(), self.val(), elements self, keys self);
    }
}

#! diamond inheritance
class DiamondRoot {
    public {
        int root_pub = 1;
    }
    private {
        int root_priv = 2;
    }
    enumerateRoot(string label) {
        fmt("DiamondRoot method on " + label, self.keys(), self.size(), self.empty(), self.val(), elements self,
            keys self);
    }
}
class DiamondLeft inherits DiamondRoot {
    public {
        int left_pub = 3;
    }
}
class DiamondRight inherits DiamondRoot {
    private {
        int right_priv = 4;
    }
}
class Diamond inherits DiamondLeft, DiamondRight {
    public {
        int bottom_pub = 5;
    }
}

#! a method passes the object to a function, which runs without the class context
class FunctionFromMethod {
    public {
        int shown = 1;
    }
    private {
        int hidden = 2;
    }
    callFunction() {
        outside(self, "FunctionFromMethod through a function called from its method");
    }
    code getClosure() {
        return sub () {
            fmt("closure created in a method", self.keys(), self.size(), self.empty(), self.val(), elements self,
                keys self);
        };
    }
    static runCode(code c) {
        c();
    }
}

#! a parent with only private:internal members and a child with only public members
class InternalOnlyParent {
    private:internal {
        int hidden = 1;
    }
    enumerateParent(string label) {
        fmt("InternalOnlyParent method on " + label, self.keys(), self.size(), self.empty(), self.val(),
            elements self, keys self);
    }
}
class PublicOfInternal inherits InternalOnlyParent {
    public {
        int shown = 2;
    }
}

#! a class whose only member is internal: its method sees it through the internal data alone
class OnlyInternal {
    private:internal {
        int a = 1;
        int b = 2;
    }
    inside() {
        fmt("OnlyInternal method", self.keys(), self.size(), self.empty(), self.val(), elements self, keys self);
    }
}

#! a child's method stores its private:internal member on an object of its parent, whose members are all public
class PublicParent {
    public {
        int a = 1;
    }
    static look(object o, string label) {
        fmt("PublicParent method on " + label, o.keys(), o.size(), o.empty(), o.val(), elements o, keys o);
    }
}
class SiblingOfInternal inherits PublicParent {
    static lookSibling(object o, string label) {
        fmt("sibling class method on " + label, o.keys(), o.size(), o.empty(), o.val(), elements o, keys o);
    }
}
class InternalOnParent inherits PublicParent {
    private:internal {
        int x;
    }
    check() {
        PublicParent p();
        object o = p;
        o.x = 2;
        checkSnapshots(keys o, hash(o), new ObjectIterator(o), new ObjectKeyIterator(o),
            new ObjectPairIterator(o), new ObjectReverseIterator(o), new ObjectKeyReverseIterator(o),
            new ObjectPairReverseIterator(o), o.pairIterator());
        fmt("child method on a parent object with its internal member", o.keys(), o.size(), o.empty(), o.val(),
            elements o, keys o);
        outside(o, "a parent object with a child's internal member");
        Unrelated::look(o, "a parent object with a child's internal member");
        PublicParent::look(o, "a parent object with a child's internal member");
        SiblingOfInternal::lookSibling(o, "a parent object with a child's internal member");
        remove o.x;
        fmt("child method after removing its internal member", o.keys(), o.size(), o.empty(), o.val(), elements o,
            keys o);
        o.x = 3;
        delete p;
        try {
            o.keys();
            printf("deleted object with internal data: no error\n");
        } catch (hash<ExceptionInfo> ex) {
            printf("deleted object with internal data: %s\n", ex.err);
        }
        # the operator is not a method call: it reaches the count of the object itself
        printf("deleted object with internal data: elements: %d\n", elements o);
    }
}

#! the same with a parent that declares no members
class EmptyParent {
}
class InternalOnEmptyParent inherits EmptyParent {
    private:internal {
        int x;
    }
    check() {
        EmptyParent p();
        object o = p;
        o.x = 2;
        fmt("child method on an empty parent object with its internal member", o.keys(), o.size(), o.empty(), o.val(),
            elements o, keys o);
        outside(o, "an empty parent object with a child's internal member");
    }
}

class Unrelated {
    static look(object o, string label) {
        fmt("Unrelated method on " + label, o.keys(), o.size(), o.empty(), o.val(), elements o, keys o);
    }
}

sub outside(object o, string label) {
    checkSnapshots(keys o, hash(o), new ObjectIterator(o), new ObjectKeyIterator(o),
        new ObjectPairIterator(o), new ObjectReverseIterator(o), new ObjectKeyReverseIterator(o),
        new ObjectPairReverseIterator(o), o.pairIterator());
    fmt("outside on " + label, o.keys(), o.size(), o.empty(), o.val(), elements o, keys o);
}

Child c();
outside(c, "Child");
c.inside();
c.insideChild();
Unrelated::look(c, "Child");
OnlyPrivate p();
outside(p, "OnlyPrivate");
p.inside();
Unrelated::look(p, "OnlyPrivate");
Base b();
outside(b, "Base");
b.inside();
PrivateChild pc();
outside(pc, "PrivateChild");
pc.insidePrivateChild();
Unrelated::look(pc, "PrivateChild");
AllPublic ap();
outside(ap, "AllPublic");
Dynamic d();
# assigned through the generic object type, which the class's (missing) declarations do not restrict
object od = d;
od.z = 3;
outside(d, "Dynamic with a member added at run time");
EnumPublicChild epc();
outside(epc, "EnumPublicChild");
epc.enumerate("EnumPublicChild");
CollideChild cc();
outside(cc, "CollideChild");
cc.enumerate("CollideChild");
cc.enumerateChild();
InternalChild ic();
outside(ic, "InternalChild");
ic.insideInternalChild();
Diamond dm();
outside(dm, "Diamond");
dm.enumerateRoot("Diamond");
Unrelated::look(dm, "Diamond");

FunctionFromMethod ffm();
ffm.callFunction();
# a closure keeps the class context of the code that created it, wherever it is called
ffm.getClosure()();
Unrelated::look(ffm, "FunctionFromMethod");
FunctionFromMethod::runCode(sub () {
    outside(ffm, "FunctionFromMethod through a closure created outside and called in its method");
});
PublicOfInternal poi();
outside(poi, "PublicOfInternal");
poi.enumerateParent("PublicOfInternal");
OnlyInternal oi();
outside(oi, "OnlyInternal");
oi.inside();
# a member added at run time to an object of a class with non-public members
OnlyPrivate pd();
object opd = pd;
opd.added = 1;
outside(pd, "OnlyPrivate with a member added at run time");
pd.inside();
# an object of another Program: its class's non-public members are not visible here
Program other(PO_NEW_STYLE);
other.parse("class Shared { public { int shared_pub = 1; } private { int shared_priv = 2; } } "
    "object sub make() { return new Shared(); }", "other");
object shared = other.callFunction("make");
outside(shared, "an object of another Program");
Unrelated::look(shared, "an object of another Program");
delete shared;
delete other;
# a deleted object: each enumeration raises the same error
object gone = new Base();
object gone_ref = gone;
delete gone;
hash<string, code> ops = {
    "keys": sub () { gone_ref.keys(); },
    "size": sub () { gone_ref.size(); },
    "empty": sub () { gone_ref.empty(); },
    "val": sub () { gone_ref.val(); },
    "firstKey": sub () { gone_ref.firstKey(); },
    "lastKey": sub () { gone_ref.lastKey(); },
};
foreach hash<auto> i in (ops.pairIterator()) {
    try {
        i.value();
        printf("deleted object: %s: no error\n", i.key);
    } catch (hash<ExceptionInfo> ex) {
        printf("deleted object: %s: %s\n", i.key, ex.err);
    }
}
InternalOnParent iop();
iop.check();
InternalOnEmptyParent ioe();
ioe.check();
