#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# members replaced or added by += in an object reached through a weak reference change the object's recursive graph as
# += on the object itself does: an object that is still referenced is not collected, and cycles made and broken are
# collected (qore#7403); needs the library debugging functions (dbg_object_external_ref()); see
# ../weak-reference-object-plus-assign.qtest

%modern
%no-child-restrictions
%allow-weak-references

class Holder {
    public {
        *Peer peer;
        static int destroyed = 0;
    }

    destructor() {
        ++Holder::destroyed;
    }
}

class Peer {
    public {
        auto holder;
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

# h -> p and p -> h; the edge from p to h is replaced by += through a weak reference to p, which stays held by h
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
            # += on the object itself, through a variable that holds no lock of h
            Peer ps = h.peer;
            ps += {"holder": NOTHING};
            break;
        }
        case "weak": wp += {"holder": NOTHING}; break;
        case "weak-value": wp += {"holder": 1, "box": "x"}; break;
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

# a cycle h -> p -> h made by += through a weak reference is collected when the local variables are released
sub made(string how) {
    int pd = Peer::destroyed;
    int hd = Holder::destroyed;
    {
        Holder h();
        Peer p();
        h.peer = p;
        Peer wp;
        wp := p;
        switch (how) {
            case "member": wp += {"holder": h}; break;
            case "nested": wp += {"box": {"h": h}}; break;
        }
    }
    printf("made %s: collected %y\n", how, Peer::destroyed == pd + 1 && Holder::destroyed == hd + 1);
}

# a cycle h -> p -> h broken by += through a weak reference is collected when the local variable is released
sub broken(string how) {
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
            case "one": wp += {"holder": NOTHING}; break;
            case "both": wp += {"holder": NOTHING, "box": NOTHING}; break;
        }
    }
    printf("broken %s: collected %y\n", how, Peer::destroyed == destroyed + 1);
}

foreach string how in ("strong", "weak", "weak-value") {
    premature(how);
}
foreach string how in ("member", "nested") {
    made(how);
}
foreach string how in ("one", "both") {
    broken(how);
}
