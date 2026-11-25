#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# Method calls made through a weak reference while another thread releases the last reference to the object
# (qore#7509): debug hooks hold the releasing thread at the points that the calls are made at, so the program only
# runs against a debug build.  After the thread has released the last reference and before it marks the object as
# being destroyed (dbg_hold_deref_last()), or after it has decided to collect the object as part of a cycle
# (dbg_hold_deref_collect()), a call takes a reference to the object: the thread abandons the deletion, the object is
# not destroyed while the call runs, and it is destroyed when the call releases it; for weak references held by a
# local variable, a global variable, a hash, a list and an object member, and for a value read from one or passed as
# an argument.  A call that entered the object before keeps it until it returns, and the destructor's own calls on
# the object are made.

%modern
%allow-weak-references

class T {
    public {
        Mutex m();
        static Counter destroyed();
        static list<string> log = ();
    }

    destructor() {
        # the destructor's own calls on the object are made
        T::log += "destructor: " + helper();
        T::destroyed.inc();
    }

    string helper() {
        AutoLock al(m);
        return "helper";
    }

    #! signals that the call has entered the object, and returns when \a go reaches zero
    int work(Counter entered, Counter go) {
        AutoLock al(m);
        entered.dec();
        go.waitForZero(HoldTimeout);
        return 1;
    }

    #! signals that the call has entered the object and waits until it may return
    int block(Counter entered, Counter go) {
        AutoLock al(m);
        entered.dec();
        go.waitForZero(30s);
        return 2;
    }
}

#! an object of a cycle (with itself through another), collected when it becomes unreachable
class C inherits T {
    public {
        *C other;
        # more references to the other object of the cycle
        list<auto> others = ();
    }

    #! releases the first \a n references of others
    clear(int n) {
        splice others, 0, n;
    }

    #! breaks the cycle from inside the object, so that it leaves its recursive set, then releases the references
    #! of the other object to it, and returns when \a go reaches zero
    int breakCycle(Counter entered, Counter go) {
        AutoLock al(m);
        # the other object, read from a weak reference
        list<auto> l = (gwb,);
        other = NOTHING;
        l[0].other = NOTHING;
        l[0].others = ();
        entered.dec();
        go.waitForZero(HoldTimeout);
        return 1;
    }
}

# a weak reference to the other object of a cycle
our C gwb;

#! an object of a cycle that is made while the object has a real reference, so that its scan is deferred
class D inherits T {
    public {
        *D other;
    }

    link(D b) {
        other = b;
    }
}

# the real reference to the object of superseded_cycle()
our D gd;

#! holds a weak reference in a member
class WeakHolder {
    public {
        T w;
    }
}

const HoldTimeout = 30s;
# how long a destroying thread that the call keeps from completing is waited for (without the fix of qore#7509, the
# destructor waits for the lock that the call holds)
const DoneTimeout = 5s;

# weak references held by a global variable
our T gw;

#! calls the method on an object passed as an argument
int sub call_arg(T o, Counter entered, Counter go) {
    return o.work(entered, go);
}

const Kinds = ("local", "global", "hash", "list", "member", "value", "argument");

#! releases the last reference to an object in another thread, which the debug hook \a hook holds; calls a method
#! through a weak reference to it meanwhile, held as \a kind, in a third thread
/** A call that is refused returns; one that enters the object keeps it, and the destroying thread is let continue
    while the call runs: the call's release of the object, which then deletes it, waits for the dereference of that
    thread to complete
*/
sub call_while_destroyed(string name, string hook, string kind) {
    string label = sprintf("%s %s", name, kind);
    Counter held(1);
    Counter release(1);
    Counter done(1);
    int destroyed = T::destroyed.getCount();
    # the weak references are made before the object is released: one made while its last reference is being
    # released waits until that dereference has completed
    T w;
    hash<auto> h;
    list<auto!> l = (NOTHING,);
    WeakHolder o();
    {
        T t();
        w := t;
        gw := t;
        h.w := t;
        l[0] := t;
        o.w := t;
        call_function(hook, "T", held, release);
        background sub () {
            on_exit done.dec();
            remove t;
        }();
    }
    if (held.waitForZero(HoldTimeout)) {
        printf("%s: the destroying thread was not held\n", label);
        release.dec();
        done.waitForZero();
        return;
    }
    Counter entered(1);
    # the call returns once the destroying thread has completed its dereference
    Counter go(1);
    Queue result();
    # the caller's values are released when its thread ends
    Counter caller_done(1);
    background sub () {
        on_exit caller_done.dec();
        try {
            int rv;
            # the call is made through the weak reference held as kind
            switch (kind) {
                case "local": rv = w.work(entered, go); break;
                case "global": rv = gw.work(entered, go); break;
                case "hash": rv = h.w.work(entered, go); break;
                case "list": rv = l[0].work(entered, go); break;
                case "member": rv = o.w.work(entered, go); break;
                case "value": {
                    # a value read from the weak reference holds the object as any value does
                    T s = w;
                    rv = s.work(entered, go);
                    break;
                }
                case "argument": rv = call_arg(w, entered, go); break;
            }
            result.push(sprintf("call returned %y", rv));
        } catch (hash<ExceptionInfo> ex) {
            result.push(sprintf("%s: %s", ex.err, ex.desc));
        }
    }();
    if (entered.waitForZero(HoldTimeout)) {
        printf("%s: the call did not enter the object\n", label);
    }
    # the destroying thread continues while the call runs: it does not destroy the object then
    release.dec();
    if (done.waitForZero(DoneTimeout)) {
        printf("%s: the destroying thread did not complete while the call runs\n", label);
    }
    printf("%s: destroyed while the call runs: %d\n", label, T::destroyed.getCount() - destroyed);
    go.dec();
    printf("%s: %s\n", label, result.get());
    done.waitForZero();
    caller_done.waitForZero();
    remove gw;
    printf("%s: destroyed: %d\n", label, T::destroyed.getCount() - destroyed);
}

#! a call made through a weak reference before the last reference is released keeps the object until it returns
sub entered_before() {
    int destroyed = T::destroyed.getCount();
    Counter entered(1);
    Counter go(1);
    Counter done(1);
    T w;
    {
        T t();
        w := t;
        background sub () {
            on_exit done.dec();
            printf("entered before: call returned %y\n", w.block(entered, go));
        }();
        if (entered.waitForZero(HoldTimeout)) {
            printf("entered before: the call did not enter the object\n");
            go.dec();
            done.waitForZero();
            return;
        }
        # the last strong reference is released while the call runs
        remove t;
    }
    printf("entered before: destroyed while the call runs: %d\n", T::destroyed.getCount() - destroyed);
    go.dec();
    done.waitForZero();
    printf("entered before: destroyed after the call: %d\n", T::destroyed.getCount() - destroyed);
}

#! the thread that releases the last reference from outside a cycle decides to collect it and is held; a call
#! through a weak reference to an object of the cycle meanwhile keeps it, and the cycle is collected afterwards; with
#! \a released, references to the object that the cycle held when the collection was decided are released while the
#! call runs (by a call through a weak reference to the other object), as many as the call holds: the count of the
#! object is then the one with which the collection was decided
sub collected_cycle(int released = 0) {
    string label = released ? sprintf("cycle released %d", released) : "cycle";
    int destroyed = T::destroyed.getCount();
    Counter held(1);
    Counter release(1);
    Counter done(1);
    C w;
    C wb;
    {
        C a();
        C b();
        a.other = b;
        b.other = a;
        map b.others += a, xrange(released);
        w := a;
        wb := b;
        remove b;
        call_function("dbg_hold_deref_collect", "C", held, release);
        background sub () {
            on_exit done.dec();
            remove a;
        }();
    }
    if (held.waitForZero(HoldTimeout)) {
        printf("%s: the collecting thread was not held\n", label);
        release.dec();
        done.waitForZero();
        return;
    }
    Counter entered(1);
    Counter go(1);
    Queue result();
    Counter caller_done(1);
    background sub () {
        on_exit caller_done.dec();
        try {
            result.push(sprintf("call returned %y", w.work(entered, go)));
        } catch (hash<ExceptionInfo> ex) {
            result.push(sprintf("%s: %s", ex.err, ex.desc));
        }
    }();
    if (entered.waitForZero(HoldTimeout)) {
        printf("%s: the call did not enter the object\n", label);
    }
    if (released) {
        wb.clear(released);
    }
    release.dec();
    if (done.waitForZero(DoneTimeout)) {
        printf("%s: the collecting thread did not complete while the call runs\n", label);
    }
    printf("%s: destroyed while the call runs: %d\n", label, T::destroyed.getCount() - destroyed);
    go.dec();
    printf("%s: %s\n", label, result.get());
    done.waitForZero();
    caller_done.waitForZero();
    # the object was kept by the call; the cycle is collected once the call has released it
    printf("%s: destroyed: %d\n", label, T::destroyed.getCount() - destroyed);
}

#! the thread that releases the last reference from outside a cycle decides to collect it and is held; a call
#! through a weak reference to an object of the cycle meanwhile breaks the cycle, so that the object leaves its
#! recursive set, and the references that the cycle held are released: the call keeps the object
sub broken_cycle() {
    int destroyed = T::destroyed.getCount();
    Counter held(1);
    Counter release(1);
    Counter done(1);
    C w;
    {
        C a();
        C b();
        a.other = b;
        b.other = a;
        b.others += a;
        w := a;
        gwb := b;
        remove b;
        call_function("dbg_hold_deref_collect", "C", held, release);
        background sub () {
            on_exit done.dec();
            remove a;
        }();
    }
    if (held.waitForZero(HoldTimeout)) {
        printf("broken cycle: the collecting thread was not held\n");
        release.dec();
        done.waitForZero();
        return;
    }
    Counter entered(1);
    Counter go(1);
    Queue result();
    Counter caller_done(1);
    background sub () {
        on_exit caller_done.dec();
        try {
            result.push(sprintf("call returned %y", w.breakCycle(entered, go)));
        } catch (hash<ExceptionInfo> ex) {
            result.push(sprintf("%s: %s", ex.err, ex.desc));
        }
    }();
    if (entered.waitForZero(HoldTimeout)) {
        printf("broken cycle: the call did not enter the object\n");
    }
    release.dec();
    if (done.waitForZero(DoneTimeout)) {
        printf("broken cycle: the collecting thread did not complete while the call runs\n");
    }
    printf("broken cycle: destroyed while the call runs: %d\n", T::destroyed.getCount() - destroyed);
    go.dec();
    printf("broken cycle: %s\n", result.get());
    done.waitForZero();
    caller_done.waitForZero();
    remove gwb;
    printf("broken cycle: destroyed: %d\n", T::destroyed.getCount() - destroyed);
}

#! the dereference that decides to collect a cycle releases the object's last real reference, and its count is
#! superseded by releases that another thread makes, in the lock-free path, before the object's set is made: a call
#! through a weak reference after the decision keeps the object
sub superseded_cycle() {
    int destroyed = T::destroyed.getCount();
    D w;
    # references to the object that are not real ones, released in the lock-free path
    list<auto> extra = ();
    {
        D a();
        # the other object is held by a list only (no real reference): its scan is made now and finds no cycle
        list<auto> bl = (new D(),);
        bl[0].other = a;
        w := a;
        gd = a;
        map extra += a, xrange(2);
        remove a;
        # the cycle is made while the call holds a real reference to the object: its scan is deferred
        gd.link(bl[0]);
    }
    Counter decide_held(1);
    Counter decide_release(1);
    Counter held(1);
    Counter release(1);
    Counter done(1);
    call_function("dbg_hold_deref_decide", "D", decide_held, decide_release);
    call_function("dbg_hold_deref_collect", "D", held, release);
    background sub () {
        on_exit done.dec();
        # the last real reference: this dereference makes the deferred scan and decides with the count it leaves
        remove gd;
    }();
    if (decide_held.waitForZero(HoldTimeout)) {
        printf("superseded cycle: the dereference did not decide\n");
        decide_release.dec();
        release.dec();
        done.waitForZero();
        return;
    }
    # released in the lock-free path: the object has no set yet
    extra = ();
    decide_release.dec();
    if (held.waitForZero(HoldTimeout)) {
        printf("superseded cycle: the collecting thread was not held\n");
        release.dec();
        done.waitForZero();
        return;
    }
    Counter entered(1);
    Counter go(1);
    Queue result();
    Counter caller_done(1);
    background sub () {
        on_exit caller_done.dec();
        try {
            result.push(sprintf("call returned %y", w.work(entered, go)));
        } catch (hash<ExceptionInfo> ex) {
            result.push(sprintf("%s: %s", ex.err, ex.desc));
        }
    }();
    if (entered.waitForZero(HoldTimeout)) {
        printf("superseded cycle: the call did not enter the object\n");
    }
    release.dec();
    if (done.waitForZero(DoneTimeout)) {
        printf("superseded cycle: the collecting thread did not complete while the call runs\n");
    }
    printf("superseded cycle: destroyed while the call runs: %d\n", T::destroyed.getCount() - destroyed);
    go.dec();
    printf("superseded cycle: %s\n", result.get());
    done.waitForZero();
    caller_done.waitForZero();
    printf("superseded cycle: destroyed: %d\n", T::destroyed.getCount() - destroyed);
}

if (!Option::HAVE_LIBRARY_DEBUGGING) {
    printf("the debug hooks are only in debug builds\n");
    exit(0);
}

foreach string kind in (Kinds) {
    call_while_destroyed("last released", "dbg_hold_deref_last", kind);
}
entered_before();
collected_cycle();
# a call through a weak reference holds two references to its object (the receiver's and the call's)
collected_cycle(2);
broken_cycle();
superseded_cycle();
printf("destructor calls: %d\n", (select T::log, $1 == "destructor: helper").size());
