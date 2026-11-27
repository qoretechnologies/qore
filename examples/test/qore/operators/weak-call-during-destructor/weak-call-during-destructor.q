#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# Method calls that another thread makes on an object while its destructor runs, after its last reference was
# released in the thread that runs it (qore#7509): the call is refused with OBJECT-ALREADY-DELETED, as a call on a
# deleted object is, so it never runs while the object's members are deleted; the destructor's own calls are made, and
# so are calls made while an explicit delete runs the destructor, as every holder of a strong reference can make them.
# Needs no debug hooks: the destructor waits until the call has been made.

%modern
%allow-weak-references

#! calls a method of an object from code that is not a method of the object
class Helper {
    static string call(T o) {
        return o.plain();
    }
}

class T {
    public {
        Mutex m();
        string name;
        int count = 0;
        *T other;
        # decremented when the destructor starts
        Counter in_dtor(1);
        # the destructor returns when this reaches zero
        Counter dtor_go(1);
        bool hold;
        *Queue events;
    }

    constructor(string name, *Queue events, bool hold = True) {
        self.name = name;
        self.events = events;
        self.hold = hold;
    }

    destructor() {
        # the destructor's own calls on the object are made: on itself, and from code of another object
        if (events) {
            events.push(sprintf("%s: destructor calls: %s %s", name, plain(), Helper::call(self)));
        }
        in_dtor.dec();
        if (hold) {
            dtor_go.waitForZero(HoldTimeout);
        }
    }

    string plain() {
        return name;
    }

    #! a setter (AOT code assigns the member on a fast path)
    string setName(string n) {
        name = n;
        return name;
    }

    #! a compound assignment of a member (AOT code changes the member on a fast path)
    int add(int i) {
        count += i;
        return count;
    }

    #! holds the member lock while it runs
    string work() {
        AutoLock al(m);
        return name;
    }
}

const HoldTimeout = 30s;

#! calls the method on an object passed as an argument
string sub call_arg(T o) {
    return o.work();
}

#! calls methods of \a o that set or change a member, with a receiver of its exact class (which AOT code calls on fast
#! paths that change the member themselves)
sub typed_calls(string label, T o) {
    try {
        printf("%s: setter call: %y\n", label, o.setName("changed"));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: setter call: %s\n", label, ex.err);
    }
    try {
        printf("%s: compound call: %y\n", label, o.add(1));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: compound call: %s\n", label, ex.err);
    }
}

#! another thread destroys an object, and this thread calls methods of the object while the destructor runs
/** @param how "last": the other thread releases the last reference; "cycle": it releases the last reference from
    outside a cycle, which is collected; "delete": it deletes the object explicitly, with references remaining
*/
sub call_during_destructor(string how) {
    Queue events();
    Counter done(1);
    T w;
    Counter in_dtor;
    Counter dtor_go;
    # the other thread takes the object from the Queue once this thread has released its own references, so that
    # the deciding release is the other thread's
    Queue handoff();
    # for the explicit deletion: a strong reference that remains
    *T strong;
    {
        T t(how, events);
        in_dtor = t.in_dtor;
        dtor_go = t.dtor_go;
        w := t;
        if (how == "cycle") {
            T b("other", NOTHING, False);
            t.other = b;
            b.other = t;
            # released here, not when the frame ends (which can be later in native code), so that the other thread's
            # release decides to collect the cycle
            remove b;
        } else if (how == "delete") {
            strong = t;
        }
        handoff.push(t);
        remove t;
    }
    background sub () {
        on_exit done.dec();
        T t = handoff.get();
        if (how == "delete") {
            delete t;
        } else {
            remove t;
        }
    }();
    if (in_dtor.waitForZero(HoldTimeout)) {
        printf("%s: the destructor did not start\n", how);
    }
    # the calls are made while the destructor runs in the other thread
    try {
        printf("%s: weak call: %y\n", how, w.work());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: weak call: %s: %s\n", how, ex.err, ex.desc);
    }
    try {
        T s = w;
        printf("%s: call through a value: %y\n", how, s.work());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: call through a value: %s\n", how, ex.err);
    }
    try {
        printf("%s: call with an argument: %y\n", how, call_arg(w));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: call with an argument: %s\n", how, ex.err);
    }
    if (how == "delete") {
        typed_calls(how, strong);
    } else {
        typed_calls(how, w);
    }
    dtor_go.dec();
    done.waitForZero();
    while (events.size()) {
        printf("%s: %s\n", how, events.get());
    }
    # after the deletion, every call is refused
    try {
        printf("%s: call after the deletion: %y\n", how, w.plain());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: call after the deletion: %s\n", how, ex.err);
    }
}

#! the reproduction of qore#7509: the destructor returns once the call has been made; the call is refused, so no
#! member lock is held when the members are deleted (no LOCK-ERROR)
sub issue_repro() {
    Counter dtor_done(1);
    Counter in_call(1);
    Counter in_dtor;
    code c;
    Queue handoff();
    {
        T t("repro");
        in_dtor = t.in_dtor;
        # the call lets the destructor return once it has been made
        Counter dtor_go = t.dtor_go;
        T w := t;
        c = string sub () {
            on_exit dtor_go.dec();
            return w.work();
        };
        handoff.push(t);
        remove t;
    }
    background sub () {
        on_exit dtor_done.dec();
        T t = handoff.get();
        remove t;
    }();
    in_dtor.waitForZero(HoldTimeout);
    try {
        printf("repro: weak call: %y\n", c());
    } catch (hash<ExceptionInfo> ex) {
        printf("repro: weak call: %s: %s\n", ex.err, ex.desc);
    }
    dtor_done.waitForZero();
}

class Doc {
    public {
        Queue entered();
        Queue proceed();
    }

    destructor() {
        entered.push(True);
        proceed.get();
    }

    string name() {
        return "doc";
    }
}

class Holder {
    public {
        *Doc doc;
    }

    constructor(Doc doc) {
        self.doc := doc;
    }
}

#! the second reproduction of qore#7509: a strong reference read from a weak reference while the destructor runs does
#! not keep the object; a call made with it is refused, during the destructor and after it
sub value_repro() {
    Doc doc();
    Queue entered = doc.entered;
    Queue proceed = doc.proceed;
    Holder holder(doc);
    Queue handoff();
    handoff.push(doc);
    remove doc;

    Counter done(1);
    background sub () {
        on_exit done.dec();
        Doc d = handoff.get();
        remove d;
    }();

    # the destructor is running in the other thread
    entered.get();
    *Doc d = holder.doc;
    printf("value: the weak reference gives an object while it is destroyed: %y\n", exists d);
    try {
        printf("value: call during the destructor: %s\n", d.name());
    } catch (hash<ExceptionInfo> ex) {
        printf("value: call during the destructor: %s: %s\n", ex.err, ex.desc);
    }
    proceed.push(True);
    done.waitForZero();
    try {
        printf("value: call after the destructor: %s\n", d.name());
    } catch (hash<ExceptionInfo> ex) {
        printf("value: call after the destructor: %s: %s\n", ex.err, ex.desc);
    }
}

foreach string how in ("last", "cycle", "delete") {
    call_during_destructor(how);
}
issue_repro();
value_repro();
