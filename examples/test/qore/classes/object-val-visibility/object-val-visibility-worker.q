#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# the value and the first and last keys of objects are not changed by the fix of the member enumerations (qore#7048)
# Copyright (C) 2026 Qore Technologies, s.r.o.

%modern

#! prints the results that the caller computed in its own code, whose class context they could depend on
sub fmt(string label, bool val, bool ternary, bool converted, *string first, *string last) {
    printf("%s: val(): %y ternary: %y boolean(): %y firstKey(): %y lastKey(): %y\n", label, val, ternary, converted,
        first, last);
}

class NoMembers {
    inside() {
        fmt("NoMembers method", self.val(), self ? True : False, boolean(self),
            self.firstKey(), self.lastKey());
    }
}

class PublicOnly {
    public {
        int a = 1;
    }
    inside() {
        fmt("PublicOnly method", self.val(), self ? True : False, boolean(self),
            self.firstKey(), self.lastKey());
    }
}

class PrivateOnly {
    private {
        int a = 1;
    }
    inside() {
        fmt("PrivateOnly method", self.val(), self ? True : False, boolean(self),
            self.firstKey(), self.lastKey());
    }
}

class InternalOnly {
    private:internal {
        int a = 1;
    }
    inside() {
        fmt("InternalOnly method", self.val(), self ? True : False, boolean(self),
            self.firstKey(), self.lastKey());
    }
}

class PublicAndPrivate {
    public {
        int a = 1;
    }
    private {
        int b = 2;
    }
    inside() {
        fmt("PublicAndPrivate method", self.val(), self ? True : False, boolean(self),
            self.firstKey(), self.lastKey());
    }
}

class PrivateChild inherits PrivateOnly {
    insideChild() {
        fmt("PrivateChild method", self.val(), self ? True : False, boolean(self),
            self.firstKey(), self.lastKey());
    }
}

class InternalChild inherits InternalOnly {
    insideChild() {
        fmt("InternalChild method", self.val(), self ? True : False, boolean(self),
            self.firstKey(), self.lastKey());
    }
}

#! members of every kind: the first and last keys do not include the class's private:internal members
class AllKinds {
    public {
        int a = 1;
    }
    private {
        int b = 2;
    }
    private:internal {
        int c = 3;
    }
    inside() {
        fmt("AllKinds method", self.val(), self ? True : False, boolean(self),
            self.firstKey(), self.lastKey());
    }
}

#! classes with every kind of member and of inheritance, for the first and last keys
class KeysBase {
    public {
        int a = 1;
    }
    private {
        int b = 2;
    }
    private:internal {
        int c = 3;
    }
    fromBase(string label) {
        fmt("KeysBase method on " + label, self.val(), self ? True : False, boolean(self),
            self.firstKey(), self.lastKey());
    }
}

class KeysPrivateChild inherits private KeysBase {
    public {
        int d = 4;
    }
    inside() {
        fmt("KeysPrivateChild method", self.val(), self ? True : False, boolean(self),
            self.firstKey(), self.lastKey());
        fromBase("KeysPrivateChild");
    }
}

class KeysInternalChild inherits private:internal KeysBase {
    public {
        int e = 5;
    }
    inside() {
        fmt("KeysInternalChild method", self.val(), self ? True : False, boolean(self),
            self.firstKey(), self.lastKey());
        fromBase("KeysInternalChild");
    }
}

class KeysGrandChild inherits KeysInternalChild {
    public {
        int f = 6;
    }
    insideGrandChild() {
        fmt("KeysGrandChild method", self.val(), self ? True : False, boolean(self),
            self.firstKey(), self.lastKey());
    }
}

#! a public member of the child with the name of the parent's private:internal member
class KeysCollideParent {
    private:internal {
        int x = 1;
    }
    public {
        int y = 2;
    }
    fromParent() {
        fmt("KeysCollideParent method", self.val(), self ? True : False, boolean(self),
            self.firstKey(), self.lastKey());
    }
}
class KeysCollideChild inherits KeysCollideParent {
    public {
        int x = 3;
    }
}

class KeysRoot {
    public {
        int r = 1;
    }
    private {
        int s = 2;
    }
    fromRoot() {
        fmt("KeysRoot method", self.val(), self ? True : False, boolean(self), self.firstKey(), self.lastKey());
    }
}
class KeysLeft inherits KeysRoot {
    public {
        int l = 3;
    }
}
class KeysRight inherits KeysRoot {
    private {
        int t = 4;
    }
}
class KeysDiamond inherits KeysLeft, KeysRight {
}

#! a child's method stores its private:internal member on an object of its parent
class KeysPublicParent {
    public {
        int a = 1;
    }
}
class KeysInternalOnParent inherits KeysPublicParent {
    private:internal {
        int x;
    }
    check() {
        KeysPublicParent p();
        object o = p;
        o.x = 2;
        fmt("child method on a parent object with its internal member", o.val(), o ? True : False, boolean(o),
            o.firstKey(), o.lastKey());
        outside(o, "a parent object with a child's internal member");
    }
}

class Unrelated {
    static look(object o, string label) {
        fmt("Unrelated method on " + label, o.val(), o ? True : False, boolean(o),
            o.firstKey(), o.lastKey());
    }
}

sub outside(object o, string label) {
    fmt("outside on " + label, o.val(), o ? True : False, boolean(o),
            o.firstKey(), o.lastKey());
}

sub check(object o, string label) {
    outside(o, label);
    Unrelated::look(o, label);
}

NoMembers n();
check(n, "NoMembers");
n.inside();
PublicOnly pub();
check(pub, "PublicOnly");
pub.inside();
PrivateOnly priv();
check(priv, "PrivateOnly");
priv.inside();
InternalOnly intern();
check(intern, "InternalOnly");
intern.inside();
PublicAndPrivate both();
check(both, "PublicAndPrivate");
both.inside();
PrivateChild pc();
check(pc, "PrivateChild");
pc.inside();
pc.insideChild();
InternalChild ic();
check(ic, "InternalChild");
ic.inside();
ic.insideChild();
# a member added at run time
NoMembers dyn();
object od = dyn;
od.z = 1;
check(dyn, "NoMembers with a member added at run time");
dyn.inside();
AllKinds all();
check(all, "AllKinds");
all.inside();
KeysPrivateChild kpc();
check(kpc, "KeysPrivateChild");
kpc.inside();
KeysInternalChild kic();
check(kic, "KeysInternalChild");
kic.inside();
KeysGrandChild kgc();
check(kgc, "KeysGrandChild");
kgc.inside();
kgc.insideGrandChild();
KeysCollideChild kcc();
check(kcc, "KeysCollideChild");
kcc.fromParent();
KeysDiamond kd();
check(kd, "KeysDiamond");
kd.fromRoot();
KeysInternalOnParent kiop();
kiop.check();
