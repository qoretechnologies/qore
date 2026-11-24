#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# the classes of the module OwnInternalTypedCallA declared in one program; own-private-internal-method-typed-call.qtest
# compares the output of every execution mode, of qcc executables and of module-user.q, which loads them from the
# module, with probe.expected

%modern

namespace OITC {
# B has a private:internal variant of m(), so its code calls its own method m() first, also on an object of a subclass
# that overrides m(): a typed call, a call reference and the untyped call call the same variant (qore#7440)
class B {
    private:internal int m(string s) { return 1; }
    string m(softstring s) { return "B::m(softstring)"; }
    static auto typedK(K k) { string s = "x"; return k.m(s); }
    static auto typedK2(K2 k) { string s = "x"; return k.m(s); }
    static auto typedC(C c) { string s = "x"; return c.m(s); }
    static string overloadC(C c) { string s = "x"; return f(c.m(s)); }
    static auto refK(K k) { code c = \k.m(); return c("x"); }
    static auto refC(C c) { code r = \c.m(); return r("x"); }
    static auto untyped(auto o) { string s = "x"; return o.m(s); }
}
# a privately inherited subclass that overrides the public variant
class K inherits private B {
    string m(softstring s) { return "K::m(softstring)"; }
}
# a publicly inherited one
class K2 inherits B {
    string m(softstring s) { return "K2::m(softstring)"; }
}
# a subclass whose own method is created by resolving its abstract methods (qore#7411)
class G0 { abstract string m(softstring s); }
class C inherits B, G0 {}
# without private:internal variants, the subclass's method is called, as before
class N {
    string m(softstring s) { return "N::m(softstring)"; }
    static auto typedNK(NK k) { string s = "x"; return k.m(s); }
    static auto untyped(auto o) { string s = "x"; return o.m(s); }
}
class NK inherits N {
    string m(softstring s) { return "NK::m(softstring)"; }
}
# the code of a class calls its own method only on an object of a class that inherits it: on an object of a parent
# class, the parent class's method is called, also by the untyped call (a decorator holding an object of its parent
# class)
class PA {
    string m(string s) { return "PA::m(string)"; }
}
class PB inherits PA {
    private { PA delegate; }
    constructor(PA d) { delegate = d; }
    private:internal string m(int i) { return "PB::m(int) private:internal"; }
    string m(string s) { return "PB(" + delegate.m(s) + ")"; }
    static auto typedA(PA a) { return a.m("k"); }
    static auto untypedA(auto a) { return a.m("k"); }
    static auto refA(PA a) { code r = \a.m(); return r("k"); }
}
string sub f(int i) { return "f(int)"; }
string sub f(string s) { return "f(string)"; }
}

%include users.ql
