#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

%modern

# issue #6056: a call on an object whose arguments delete the object raises OBJECT-ALREADY-DELETED in every mode, also
# when the call is made to a member holding a closure, to methodGate(), to a pseudo-method of <object> or to a static
# method of the class of the object; a call on an object deleted before the call raises it too, and a call whose
# arguments leave the object alone is made

class MemberClosure {
    public {
        code f = string sub (auto a) { return sprintf("closure %y", a); };
        code r = sub (reference<int> v, int n) { v = n; };
        int i = 1;
    }
}

class GateClass {
    string methodGate(string name, auto a) {
        return sprintf("gate %s(%y)", name, a);
    }
}

class GateChild inherits GateClass {
}

class StrictGate {
    string methodGate(string name, int a) {
        return sprintf("strict gate %s(%y)", name, a);
    }
}

class GateMember {
    public {
        int anything = 1;
    }

    string methodGate(string name, auto a) {
        return sprintf("member gate %s(%y)", name, a);
    }
}

class Priv {
    pub(auto a) {}
}

class St {
    static string s(auto a) { return sprintf("static %y", a); }
    static string s2(int a) { return sprintf("static2 int %y", a); }
    static string s2(string a) { return sprintf("static2 string %y", a); }

    # a call through self of a static method has no object: the method is called
    string selfCall() {
        object h = self;
        return s(del(\h));
    }

    # a call through self as an object checks it
    string selfDotCall() {
        object h = self;
        return self.s(del(\h));
    }
}

class StChild inherits St {
}

int sub del(reference<object> h) {
    delete h;
    return 1;
}

string sub dels(reference<object> h) {
    delete h;
    return "pub";
}

sub check(string label, code c) {
    try {
        printf("%s: %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

auto sub closure_untyped() {
    object o = new MemberClosure();
    object h = o;
    return o.f(del(\h));
}

auto sub closure_typed() {
    MemberClosure o = new MemberClosure();
    object h = o;
    return o.f(del(\h));
}

auto sub closure_closure_arg() {
    object o = new MemberClosure();
    object h = o;
    return o.f((int sub () { delete h; return 1; })());
}

auto sub closure_before() {
    object o = new MemberClosure();
    object h = o;
    delete h;
    return o.f(1);
}

auto sub closure_not_deleted() {
    object o = new MemberClosure();
    object other = new Priv();
    return o.f(del(\other) + 1);
}

auto sub closure_reference_arg() {
    object o = new MemberClosure();
    int v = 1;
    o.r(\v, 2);
    return v;
}

auto sub gate_untyped() {
    object o = new GateClass();
    object h = o;
    return o.anything(del(\h));
}

auto sub gate_typed() {
    GateClass o = new GateClass();
    object h = o;
    return o.anything(del(\h));
}

auto sub gate_subclass() {
    object o = new GateChild();
    object h = o;
    return o.anything(del(\h));
}

auto sub gate_closure_arg() {
    object o = new GateClass();
    object h = o;
    return o.anything((int sub () { delete h; return 1; })());
}

auto sub gate_bad_args() {
    object o = new StrictGate();
    object h = o;
    return o.anything(del(\h) ? "x" : "y");
}

auto sub gate_member_not_code() {
    object o = new GateMember();
    object h = o;
    return o.anything(del(\h));
}

auto sub gate_before() {
    object o = new GateClass();
    object h = o;
    delete h;
    return o.anything(1);
}

auto sub gate_not_deleted() {
    object o = new GateClass();
    object other = new Priv();
    return o.anything(del(\other) + 1);
}

auto sub pseudo_untyped() {
    object o = new Priv();
    object h = o;
    return o.hasCallableMethod(dels(\h));
}

auto sub pseudo_typed() {
    Priv o = new Priv();
    object h = o;
    return o.hasCallableMethod(dels(\h));
}

auto sub pseudo_closure_arg() {
    object o = new Priv();
    object h = o;
    return o.hasCallableMethod((string sub () { delete h; return "pub"; })());
}

auto sub pseudo_before() {
    object o = new Priv();
    object h = o;
    delete h;
    return o.hasCallableMethod("pub");
}

auto sub pseudo_not_deleted() {
    object o = new Priv();
    object other = new Priv();
    return o.hasCallableMethod(dels(\other));
}

auto sub static_untyped() {
    object o = new St();
    object h = o;
    return o.s(del(\h));
}

auto sub static_typed() {
    St o = new St();
    object h = o;
    return o.s(del(\h));
}

auto sub static_subclass() {
    object o = new StChild();
    object h = o;
    return o.s(del(\h));
}

auto sub static_closure_arg() {
    object o = new St();
    object h = o;
    return o.s((int sub () { delete h; return 1; })());
}

auto sub static_overloaded() {
    St o = new St();
    object h = o;
    return o.s2(del(\h));
}

auto sub static_no_variant() {
    St o = new St();
    object h = o;
    auto x = 1;
    return o.s2(x, del(\h));
}

auto sub static_self_dot() {
    return (new St()).selfDotCall();
}

auto sub static_self_call() {
    return (new St()).selfCall();
}

auto sub static_before() {
    object o = new St();
    object h = o;
    delete h;
    return o.s(1);
}

auto sub static_not_deleted() {
    object o = new St();
    object other = new Priv();
    return o.s(del(\other) + 1);
}

# member holding a closure
check("member closure, untyped, deleted by an argument", \closure_untyped());
check("member closure, typed, deleted by an argument", \closure_typed());
check("member closure, deleted by a closure argument", \closure_closure_arg());
check("member closure, deleted before", \closure_before());
check("member closure, not deleted", \closure_not_deleted());
check("member closure, reference argument", \closure_reference_arg());

# methodGate()
check("methodGate, untyped, deleted by an argument", \gate_untyped());
check("methodGate, typed, deleted by an argument", \gate_typed());
check("methodGate, subclass, deleted by an argument", \gate_subclass());
check("methodGate, deleted by a closure argument", \gate_closure_arg());
check("methodGate, arguments that it does not accept, deleted by an argument", \gate_bad_args());
check("methodGate, member that is not code, deleted by an argument", \gate_member_not_code());
check("methodGate, deleted before", \gate_before());
check("methodGate, not deleted", \gate_not_deleted());

# pseudo-method of <object>
check("pseudo-method, untyped, deleted by an argument", \pseudo_untyped());
check("pseudo-method, typed, deleted by an argument", \pseudo_typed());
check("pseudo-method, deleted by a closure argument", \pseudo_closure_arg());
check("pseudo-method, deleted before", \pseudo_before());
check("pseudo-method, not deleted", \pseudo_not_deleted());

# static method called through an object
check("static method, untyped, deleted by an argument", \static_untyped());
check("static method, typed, deleted by an argument", \static_typed());
check("static method, subclass, deleted by an argument", \static_subclass());
check("static method, deleted by a closure argument", \static_closure_arg());
check("static method, overloaded, deleted by an argument", \static_overloaded());
check("static method, no variant for the arguments, deleted by an argument", \static_no_variant());
check("static method, through self, deleted by an argument", \static_self_dot());
check("static method, call through self without an object, deleted by an argument", \static_self_call());
check("static method, deleted before", \static_before());
check("static method, not deleted", \static_not_deleted());
