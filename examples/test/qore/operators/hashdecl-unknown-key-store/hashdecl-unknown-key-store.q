#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# A key that a hashdecl does not declare, assigned to a hashdecl hash through an lvalue whose type does not name the
# hashdecl (hash<auto>, auto), raises INVALID-MEMBER in every mode, as the AST interpreter raises it (qore#7365); the
# merge of a hash with such a key raises HASHDECL-KEY-ERROR in every mode

%modern

hashdecl HD {
    int a = 1;
}

class Holder {
    public {
        hash<auto> h;
    }

    constructor() {
        h = new hash<HD>();
    }

    store(string key) {
        h{key} = 2;
    }

    storeConstant() {
        h.z = 3;
    }
}

sub show(string label, code c) {
    try {
        c();
        printf("%s: no exception\n", label);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

sub param_constant(hash<auto> h) {
    h.z = 1;
}

sub param_dynamic(hash<auto> h, string key) {
    h{key} = 1;
}

sub param_known(hash<auto> h) {
    h.a = 5;
    printf("known key: %y\n", h);
}

sub untyped_param(auto h) {
    h.z = 1;
}

# a local that no closure uses, with a constant and a dynamic key
sub local_keys() {
    hash<auto> l = make_hd();
    try {
        l.y = 1;
    } catch (hash<ExceptionInfo> ex) {
        printf("plain local, constant key: %s: %s\n", ex.err, ex.desc);
    }
    string key = "y";
    try {
        l{key} = 1;
    } catch (hash<ExceptionInfo> ex) {
        printf("plain local, dynamic key: %s: %s\n", ex.err, ex.desc);
    }
    l.a = 7;
    printf("plain local: %y\n", l);
}

auto sub make_hd() {
    return new hash<HD>();
}

sub main() {
    hash<HD> h();
    show("parameter, constant key", sub () { param_constant(h); });
    show("parameter, dynamic key", sub () { param_dynamic(h, "z"); });
    show("untyped parameter", sub () { untyped_param(h); });
    param_known(h);
    local_keys();

    # the type of the local does not name the hashdecl at parse time
    hash<auto> local = make_hd();
    show("local, constant key", sub () { local.y = 1; });
    show("local, dynamic key", sub () { local{"y"} = 1; });

    Holder o();
    show("member, constant key", \o.storeConstant());
    show("member, dynamic key", sub () { o.store("y"); });

    show("merge", sub () { local += {"y": 1}; });
    printf("unchanged: %y, %y, %y\n", h, local, o.h);
}

main();
