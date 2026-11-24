#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# two threads store values whose reference elements refer to each other's targets (qore#7391)
#
# Requires a debug build: dbg_reference_bind_rendezvous() makes the binds of the reference elements of both stores
# wait for each other, so that both are in progress at the same time.  A bind made with an lvalue write lock held (the
# lock of the target of its store, as before qore#7391) fails at once with DBG-REFERENCE-BIND-HELD instead of waiting,
# where two threads that each hold the lock of their target and lock the other's would deadlock.

%modern

#! the bound of each wait, in milliseconds; only a hang guard: no result depends on it
const Timeout = 60000;

class Holder {
    public {
        auto v;
        list<reference<softint>> r;
    }

    constructor(string v) {
        self.v = v;
    }
}

hashdecl H {
    auto v;
    list<reference<softint>> r;
}

class HashHolder {
    public {
        auto v;
        hash<string, reference<softint>> h = {};
    }

    constructor(string v) {
        self.v = v;
    }
}

# an object whose members += merges into a hash
class Source {
    public {
        auto a;
    }
}

our hash<H> g1 = cast<hash<H>>({"v": "1"});
our hash<H> g2 = cast<hash<H>>({"v": "2"});

#! runs a store in a thread and pushes its result: "ok" or the error of its exception
sub run_store(Queue q, int id, code store) {
    string result = "ok";
    try {
        store();
    } catch (hash<ExceptionInfo> ex) {
        result = ex.err;
    }
    q.push({"id": id, "result": result});
}

#! runs the stores in threads of their own, all at the same time, and prints the result
/** @param label the label of the output line
    @param stores the stores, one per thread; the binds of their reference elements wait for each other
    @param check returns the state of the targets and of the referenced values after the stores
*/
sub run(string label, list<code> stores, code check) {
    call_function("dbg_reference_bind_rendezvous", stores.size(), Timeout);
    hash<auto> rv;
    list<string> results = ();
    {
        on_exit {
            # disarms the rendezvous
            rv = call_function("dbg_reference_bind_rendezvous_result");
        }
        Queue q();
        foreach code store in (stores) {
            background run_store(q, $#, store);
        }
        for (int i = 0; i < stores.size(); ++i) {
            results += "";
        }
        for (int i = 0; i < stores.size(); ++i) {
            hash<auto> r = q.get(Timeout);
            results[r.id] = r.result;
        }
    }
    # the check reports its error, as after a store that failed, which leaves the targets without the references
    auto state;
    try {
        state = check();
    } catch (hash<ExceptionInfo> ex) {
        state = ex.err;
    }
    printf("%s: parties %d, arrived %d, held %d, max held %d, timeouts %d; stores: %y; %y\n", label, rv.parties,
        rv.arrived, rv.held, rv.max_held, rv.timeouts, results, state);
}

# members of two objects, each object's target referring to the other object's member
{
    Holder a("1");
    Holder b("2");
    list<auto> xa = (\b.v,);
    list<auto> xb = (\a.v,);
    run("members of two objects", (sub () { a.r = xa; }, sub () { b.r = xb; }), auto sub () {
        list<auto> rv = (a.r.fullType(), b.r.fullType(), a.v.type(), a.v, b.v.type(), b.v);
        # the elements refer to the other object's member
        a.r[0] = 10;
        b.r[0] = 20;
        rv += (a.v, b.v);
        # the objects refer to each other through the references: the cycles are broken
        a.r = ();
        b.r = ();
        return rv;
    });
}

# two global variables
{
    list<auto> x1 = (\g2.v,);
    list<auto> x2 = (\g1.v,);
    run("global variables", (sub () { g1.r = x1; }, sub () { g2.r = x2; }), auto sub () {
        list<auto> rv = (g1.r.fullType(), g2.r.fullType(), g1.v.type(), g1.v, g2.v.type(), g2.v);
        g1.r[0] = 10;
        g2.r[0] = 20;
        rv += (g1.v, g2.v);
        g1.r = ();
        g2.r = ();
        return rv;
    });
}

# two closure-bound variables
{
    hash<H> h1 = cast<hash<H>>({"v": "1"});
    hash<H> h2 = cast<hash<H>>({"v": "2"});
    list<auto> x1 = (\h2.v,);
    list<auto> x2 = (\h1.v,);
    run("closure-bound variables", (sub () { h1.r = x1; }, sub () { h2.r = x2; }), auto sub () {
        list<auto> rv = (h1.r.fullType(), h2.r.fullType(), h1.v.type(), h1.v, h2.v.type(), h2.v);
        h1.r[0] = 10;
        h2.r[0] = 20;
        rv += (h1.v, h2.v);
        # the variables refer to each other through the references: the cycles are broken
        h1.r = ();
        h2.r = ();
        return rv;
    });
}

# a member of the same object, guarded by the lock of the target (qore#7380), in one thread
{
    Holder a("3");
    list<auto> xa = (\a.v,);
    run("member of the same object", (sub () { a.r = xa; },), auto sub () {
        list<auto> rv = (a.r.fullType(), a.v.type(), a.v);
        a.r[0] = 30;
        rv += a.v;
        # the object refers to itself through the reference: the cycle is broken
        a.r = ();
        return rv;
    });
}

# the same in two threads at the same time, each with an object of its own
{
    Holder a("4");
    Holder b("5");
    list<auto> xa = (\a.v,);
    list<auto> xb = (\b.v,);
    run("members of the same objects", (sub () { a.r = xa; }, sub () { b.r = xb; }), auto sub () {
        list<auto> rv = (a.r.fullType(), b.r.fullType(), a.v.type(), a.v, b.v.type(), b.v);
        a.r = ();
        b.r = ();
        return rv;
    });
}

# push into members of two objects, each element referring to the other object's member
{
    Holder a("6");
    Holder b("7");
    run("push into members of two objects", (sub () { push a.r, \b.v; }, sub () { push b.r, \a.v; }), auto sub () {
        list<auto> rv = (a.r.size(), b.r.size(), a.v.type(), a.v, b.v.type(), b.v);
        a.r = ();
        b.r = ();
        return rv;
    });
}

# += of an object on the hashes of two objects, the members of each object referring to the other target's object
{
    HashHolder a("8");
    HashHolder b("9");
    Source sa();
    sa.a = \b.v;
    Source sb();
    sb.a = \a.v;
    run("hash += object in two objects", (sub () { a.h += sa; }, sub () { b.h += sb; }), auto sub () {
        list<auto> rv = (a.h.size(), b.h.size(), a.v.type(), a.v, b.v.type(), b.v);
        a.h = {};
        b.h = {};
        return rv;
    });
}
