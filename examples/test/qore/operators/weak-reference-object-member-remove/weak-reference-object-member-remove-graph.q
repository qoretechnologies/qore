#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# a member removed from an object reached through a weak reference changes the object's recursive graph as one removed
# through a strong reference does: an object that is still referenced is not collected, and a cycle broken is collected
# (qore#7346); needs the library debugging functions (dbg_object_external_ref()); see
# ../weak-reference-object-member-remove.qtest

%modern
%no-child-restrictions
%allow-weak-references
class Holder { public { *Peer peer; } }
class WeakHolder { public { *Peer w; } }
class Peer {
    public { *Holder holder; auto box; static int destroyed = 0; }
    destructor() { ++Peer::destroyed; }
    string name() { return "peer"; }
}
class Conn {
    public { *Mgr mgr; *Op op; static int destroyed = 0; }
    constructor(Mgr m) { mgr = m; op = new Op(self); }
    destructor() { ++Conn::destroyed; }
}
class Op { public { *Conn conn; static int destroyed = 0; } constructor(Conn c) { conn = c; } destructor() { ++Op::destroyed; } }
class Mgr { public { *Conn direct; hash<auto> pool = {}; } }

sub premature(string how) {
    int destroyed = Peer::destroyed;
    Holder h();
    int ext;
    {
        Peer p();
        h.peer = p;
        p.holder = h;
        ext = call_function("dbg_object_external_ref", p);
    }
    Peer wp;
    wp := h.peer;
    switch (how) {
        case "strong": remove h.peer.holder; break;
        case "weak": remove wp.holder; break;
        case "weak-slice": remove wp.("holder", "box"); break;
        case "weak-assign": wp.holder = NOTHING; break;
        case "weak-member": {
            # the weak reference is held by a member of another object
            WeakHolder c();
            c.w := h.peer;
            remove c.w.holder;
            break;
        }
    }
    remove wp;
    call_function("dbg_object_external_deref", ext);
    string name;
    try { name = h.peer.name(); } catch (hash<ExceptionInfo> ex) { name = ex.err; }
    printf("premature %s: collected early %y name %y\n", how, Peer::destroyed != destroyed, name);
    delete h;
    printf("premature %s: collected after %y\n", how, Peer::destroyed == destroyed + 1);
}

sub leak(string how) {
    int conns = Conn::destroyed;
    int ops = Op::destroyed;
    {
        Mgr m();
        {
            Conn c(m);
            m.direct = c;
            m.pool.k = c;
        }
        Mgr wm;
        wm := m;
        switch (how) {
            case "strong": remove m.direct; remove m.pool; break;
            case "weak": remove wm.direct; remove wm.pool; break;
            case "weak-slice": remove wm.("direct", "pool"); break;
        }
    }
    printf("leak %s: conn collected %y op collected %y\n", how, Conn::destroyed == conns + 1, Op::destroyed == ops + 1);
}

foreach string how in ("strong", "weak", "weak-slice", "weak-assign", "weak-member") {
    premature(how);
}
foreach string how in ("strong", "weak", "weak-slice") {
    leak(how);
}
