#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# members removed by -= from an object reached through a weak reference change the object's recursive graph as -= on
# the object itself does: an object that is still referenced is not collected, and a cycle broken is collected
# (qore#7368); needs the library debugging functions (dbg_object_external_ref()); see
# ../weak-reference-object-minus-assign.qtest

%modern
%no-child-restrictions
%allow-weak-references

class Holder {
    public {
        *Peer peer;
    }
}

class Peer {
    public {
        *Holder holder;
        auto box;
        static int destroyed = 0;
    }

    destructor() {
        ++Peer::destroyed;
    }

    string name() {
        return "peer";
    }
}

# h -> p and p -> h; the edge from p to h is removed by -= through a weak reference to p, which stays held by h
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
        case "strong": {
            # -= on the object itself, through a variable that holds no lock of h (-= through h.peer: qore#7401)
            Peer ps = h.peer;
            ps -= "holder";
            break;
        }
        case "weak": wp -= "holder"; break;
        case "weak-list": wp -= ("holder", "box"); break;
    }
    remove wp;
    call_function("dbg_object_external_deref", ext);
    string name;
    try {
        name = h.peer.name();
    } catch (hash<ExceptionInfo> ex) {
        name = ex.err;
    }
    printf("premature %s: collected early %y name %y\n", how, Peer::destroyed != destroyed, name);
    delete h;
    printf("premature %s: collected after %y\n", how, Peer::destroyed == destroyed + 1);
}

# a cycle h -> p -> h broken by -= through a weak reference is collected when the local variable is released
sub cycle(string how) {
    int destroyed = Peer::destroyed;
    {
        Holder h();
        {
            Peer p();
            h.peer = p;
            p.holder = h;
            p.box = h;
        }
        Peer wp;
        wp := h.peer;
        switch (how) {
            case "one": wp -= "holder"; break;
            case "both": wp -= ("holder", "box"); break;
        }
    }
    printf("cycle %s: collected %y\n", how, Peer::destroyed == destroyed + 1);
}

foreach string how in ("strong", "weak", "weak-list") {
    premature(how);
}
foreach string how in ("one", "both") {
    cycle(how);
}
