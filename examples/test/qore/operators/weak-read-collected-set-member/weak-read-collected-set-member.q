#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# A value of a recursive set that another thread reads through a weak reference after a dereference has decided to
# collect the set keeps the whole set (qore#7556): debug hooks hold the collecting thread after its decision
# (dbg_hold_deref_collect() for an object whose dereference decides, dbg_hold_closure_var_collect() for a
# closure-bound variable), the main thread reads another object of the set, or a closure of the set, through a weak
# reference meanwhile, and uses it once the collecting thread has completed: the set is not collected while the value
# is held, and is collected when it is released.  The program only runs against a debug build.

%modern
%allow-weak-references

class T {
    public {
        static Counter destroyed();
    }

    destructor() {
        T::destroyed.inc();
    }

    string name() {
        return "T";
    }
}

#! an object of a cycle
class C inherits T {
    public {
        *C other;
        hash<auto> h;
    }

    #! calls a method of the other object of the cycle
    string otherName() {
        return other.name();
    }
}

#! an object that holds a closure
class TC inherits T {
    public {
        *code c;
    }
}

const HoldTimeout = 30s;

# a weak reference to the hash of member_closure()
our hash<auto> wch;
# a weak reference to the object of closure_var_object()
our TC wcv;
# a weak reference to the hash of closure_var_closure()
our hash<auto> wcvh;

#! makes the cycle of an object, the hash in its member, a closure in the hash and the variable that the closure
#! captured, which holds the object
sub link_closure(C a) {
    C x = a;
    a.h = {"c": string sub () { return exists x ? x.name() : "NOTHING"; }};
    wch := a.h;
}

#! makes a cycle of a closure-bound variable, the object it holds and a closure that captured the variable, which
#! the object holds, and arms the hold of the collection of the variable, which the return releases
sub make_closure_var_object_cycle(Counter held, Counter release) {
    TC cvo();
    cvo.c = string sub () { return exists cvo ? cvo.className() : "NOTHING"; };
    wcv := cvo;
    call_function("dbg_hold_closure_var_collect", "cvo", held, release);
}

#! makes the cycle of a closure-bound variable, the hash it holds and a closure that captured it, which the hash
#! holds, and arms the hold of the collection of the variable, which the return releases
sub make_closure_var_hash_cycle(Counter held, Counter release) {
    hash<auto> cvc = {};
    cvc.t = new T();
    cvc.c = string sub () { return exists cvc ? "H" : "NOTHING"; };
    wcvh := cvc;
    call_function("dbg_hold_closure_var_collect", "cvc", held, release);
}

#! the collecting thread is held; returns False if it was not
bool sub wait_held(string label, Counter held, Counter release, Counter done) {
    if (held.waitForZero(HoldTimeout)) {
        printf("%s: the collecting thread was not held\n", label);
        release.dec();
        done.waitForZero(HoldTimeout);
        return False;
    }
    return True;
}

#! waits for the collecting thread to complete, for HoldTimeout at most
sub wait_done(string label, Counter done) {
    if (done.waitForZero(HoldTimeout)) {
        printf("%s: the collecting thread did not complete\n", label);
    }
}

#! the release of the last reference from outside a cycle of two objects decides to collect it and is held; the
#! other object, read through a weak reference meanwhile, keeps the cycle: its method that calls the first object
#! is not refused
sub member_object() {
    string label = "member object";
    int destroyed = T::destroyed.getCount();
    Counter held(1);
    Counter release(1);
    Counter done(1);
    Counter left(1);
    C wb;
    {
        C a();
        C b();
        a.other = b;
        b.other = a;
        wb := b;
        remove b;
        # the background thread releases the variable's reference once this frame's reference has been released,
        # so that its release is the one that decides to collect the cycle, which the hold holds
        background sub () {
            on_exit done.dec();
            left.waitForZero(HoldTimeout);
            call_function("dbg_hold_deref_collect", "C", held, release);
            remove a;
        }();
    }
    left.dec();
    if (!wait_held(label, held, release, done)) {
        return;
    }
    # the other object of the cycle, read after the decision
    C b = wb;
    release.dec();
    wait_done(label, done);
    printf("%s: destroyed while held: %d\n", label, T::destroyed.getCount() - destroyed);
    try {
        printf("%s: call returned %y\n", label, b.otherName());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
    remove b;
    printf("%s: destroyed: %d\n", label, T::destroyed.getCount() - destroyed);
}

#! the release of the last reference from outside the cycle of an object, a hash, a closure and the variable that the
#! closure captured decides to collect it and is held; the closure, read through a weak reference to the hash
#! meanwhile, keeps the cycle: its call reads the object from the variable
sub member_closure() {
    string label = "member closure";
    int destroyed = T::destroyed.getCount();
    Counter held(1);
    Counter release(1);
    Counter done(1);
    Counter left(1);
    {
        C a();
        link_closure(a);
        # the background thread releases the variable's reference once this frame's reference has been released,
        # so that its release is the one that decides to collect the cycle, which the hold holds
        background sub () {
            on_exit done.dec();
            left.waitForZero(HoldTimeout);
            call_function("dbg_hold_deref_collect", "C", held, release);
            remove a;
        }();
    }
    left.dec();
    if (!wait_held(label, held, release, done)) {
        return;
    }
    # the closure of the cycle, read after the decision
    code c = wch.c;
    release.dec();
    wait_done(label, done);
    printf("%s: destroyed while held: %d\n", label, T::destroyed.getCount() - destroyed);
    try {
        printf("%s: call returned %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
    remove c;
    printf("%s: destroyed: %d\n", label, T::destroyed.getCount() - destroyed);
}

#! the release of the frame's reference to a closure-bound variable decides to collect the cycle of the variable, the
#! object it holds and a closure that captured it, and is held; the object, read through a weak reference meanwhile,
#! keeps the cycle: the closure, called once the collecting thread has completed, reads the object from the variable
sub closure_var_object() {
    string label = "closure var object";
    int destroyed = T::destroyed.getCount();
    Counter held(1);
    Counter release(1);
    Counter done(1);
    background sub () {
        on_exit done.dec();
        make_closure_var_object_cycle(held, release);
    }();
    if (!wait_held(label, held, release, done)) {
        return;
    }
    # the object of the cycle, read after the decision
    TC t = wcv;
    release.dec();
    wait_done(label, done);
    printf("%s: destroyed while held: %d\n", label, T::destroyed.getCount() - destroyed);
    try {
        printf("%s: call returned %y\n", label, t.c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
    remove t;
    printf("%s: destroyed: %d\n", label, T::destroyed.getCount() - destroyed);
}

#! the release of the frame's reference to a closure-bound variable decides to collect the cycle of the variable, the
#! hash it holds and a closure that captured it, which the hash holds, and is held; the closure, read through a weak
#! reference to the hash meanwhile, keeps the cycle: its call, once the collecting thread has completed, reads the hash
#! from the variable
sub closure_var_closure() {
    string label = "closure var closure";
    int destroyed = T::destroyed.getCount();
    Counter held(1);
    Counter release(1);
    Counter done(1);
    background sub () {
        on_exit done.dec();
        make_closure_var_hash_cycle(held, release);
    }();
    if (!wait_held(label, held, release, done)) {
        return;
    }
    # the closure of the cycle, read after the decision
    code c = wcvh.c;
    release.dec();
    wait_done(label, done);
    printf("%s: destroyed while held: %d\n", label, T::destroyed.getCount() - destroyed);
    try {
        printf("%s: call returned %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
    remove c;
    printf("%s: destroyed: %d\n", label, T::destroyed.getCount() - destroyed);
}

if (!Option::HAVE_LIBRARY_DEBUGGING) {
    printf("the debug hooks are only in debug builds\n");
    exit(0);
}

member_object();
member_closure();
closure_var_object();
closure_var_closure();
