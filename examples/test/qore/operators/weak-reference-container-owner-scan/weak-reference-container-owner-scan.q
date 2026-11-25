#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# a value removed or replaced in a list or a hash that an object holds, through a weak reference to the container,
# changes the recursive graph of the object as the same change through the object does: an object that is still
# referenced is not collected, and a cycle broken is collected, in every mode (qore#7373); needs the library debugging
# functions (dbg_object_external_ref()); see ../weak-reference-container-owner-scan.qtest

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
        hash<auto> box = {};
        list<auto> lst = ();
        hash<auto> outer = {"inner": {}};
        static int destroyed = 0;
    }

    destructor() {
        ++Peer::destroyed;
    }

    string name() {
        return "peer";
    }
}

class WeakHolder {
    public {
        hash<auto> wbox;
    }
}

# h -> p and p -> h through one of p's containers; after the edge from p to h is removed through a weak reference to
# the container, p is held only by h (and by a reference from outside Qore until it is released), and h by the local
# variable: p must not be collected
sub premature(string how) {
    int destroyed = Peer::destroyed;
    Holder h();
    int ext;
    {
        Peer p();
        h.peer = p;
        switch (how) {
            case "strong":
            case "hash-remove":
            case "hash-assign":
            case "hash-held-by-member":
                p.box.holder = h;
                break;
            case "list-remove":
            case "list-assign":
                p.lst = (h, 1);
                break;
            case "nested-remove":
                p.outer.inner.holder = h;
                break;
        }
        ext = call_function("dbg_object_external_ref", p);
    }
    switch (how) {
        case "strong":
            remove h.peer.box.holder;
            break;
        case "hash-remove": {
            hash<auto> wh;
            wh := h.peer.box;
            remove wh.holder;
            break;
        }
        case "hash-assign": {
            hash<auto> wh;
            wh := h.peer.box;
            wh.holder = NOTHING;
            break;
        }
        case "hash-held-by-member": {
            # the weak reference is held by a member of another object
            WeakHolder w();
            w.wbox := h.peer.box;
            remove w.wbox.holder;
            break;
        }
        case "list-remove": {
            list<auto> wl;
            wl := h.peer.lst;
            remove wl[0];
            break;
        }
        case "list-assign": {
            list<auto> wl;
            wl := h.peer.lst;
            wl[0] = 2;
            break;
        }
        case "nested-remove": {
            hash<auto> wh;
            wh := h.peer.outer;
            remove wh.inner.holder;
            break;
        }
    }
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

# a cycle h -> p -> h through a container of p and through p's other container: removing one edge through a weak
# reference leaves the other cycle, which is collected when the local variables are released
sub cycle(string how) {
    int destroyed = Peer::destroyed;
    {
        Holder h();
        {
            Peer p();
            h.peer = p;
            p.box.holder = h;
            p.lst = (h, 1);
        }
        hash<auto> wh;
        wh := h.peer.box;
        list<auto> wl;
        wl := h.peer.lst;
        switch (how) {
            case "hash": remove wh.holder; break;
            case "list": remove wl[0]; break;
            case "both": remove wh.holder; remove wl[0]; break;
        }
    }
    printf("cycle %s: collected %y\n", how, Peer::destroyed == destroyed + 1);
}

# h -> p -> p.box.lst -> h: one edge from the list held in the container of the weak reference to h is removed by a
# list operator, or the list is replaced while something else still references it
sub list_ops(string how) {
    int destroyed = Peer::destroyed;
    Holder h();
    int ext;
    {
        Peer p();
        h.peer = p;
        p.box.lst = (h, 1);
        ext = call_function("dbg_object_external_ref", p);
    }
    hash<auto> wh;
    wh := h.peer.box;
    switch (how) {
        case "strong-shift": shift h.peer.box.lst; break;
        case "shift": shift wh.lst; break;
        case "pop": pop wh.lst; pop wh.lst; break;
        case "splice": splice wh.lst, 0, 1; break;
        case "extract": extract wh.lst, 0, 1; break;
        case "extract-value": {
            auto v = extract wh.lst, 0, 1;
            break;
        }
        case "shared-replaced": {
            # the list replaced is still referenced by another variable, but its edge from the container is gone
            list<auto> keep = wh.lst;
            wh.lst = 0;
            break;
        }
        case "foreach-reference": {
            # the list is shared with the loop while the reference writes its elements back
            foreach auto x in (\wh.lst) {
                x = 0;
            }
            break;
        }
    }
    call_function("dbg_object_external_deref", ext);
    string name;
    try {
        name = h.peer.name();
    } catch (hash<ExceptionInfo> ex) {
        name = ex.err;
    }
    printf("list op %s: collected early %y name %y\n", how, Peer::destroyed != destroyed, name);
    delete h;
    printf("list op %s: collected after %y\n", how, Peer::destroyed == destroyed + 1);
}

# h -> p -> p.lst -> h: a range of a list that p holds removed through a weak reference to the list
sub list_range() {
    int destroyed = Peer::destroyed;
    Holder h();
    int ext;
    {
        Peer p();
        h.peer = p;
        p.lst = (h, 1);
        ext = call_function("dbg_object_external_ref", p);
    }
    list<auto> wl;
    wl := h.peer.lst;
    remove wl[0..0];
    call_function("dbg_object_external_deref", ext);
    string name;
    try {
        name = h.peer.name();
    } catch (hash<ExceptionInfo> ex) {
        name = ex.err;
    }
    printf("list range: collected early %y name %y\n", Peer::destroyed != destroyed, name);
    delete h;
    printf("list range: collected after %y\n", Peer::destroyed == destroyed + 1);
}

foreach string how in ("strong", "hash-remove", "hash-assign", "hash-held-by-member", "list-remove", "list-assign",
        "nested-remove") {
    premature(how);
}
foreach string how in ("strong-shift", "shift", "pop", "splice", "extract", "extract-value", "shared-replaced",
        "foreach-reference") {
    list_ops(how);
}
list_range();
foreach string how in ("hash", "list", "both") {
    cycle(how);
}
