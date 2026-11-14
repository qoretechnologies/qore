#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# on_exit handlers nested 16 deep in the bodies of other handlers, and 12 deep in a function whose handlers raise
# exceptions and run out of line (#6087): their lowering took time exponential in the nesting; the output is the same
# in every execution mode

%modern
%new-style
%strict-args
%require-types
%enable-all-warnings

our list<auto> hl;

auto sub nested_return(int x) {
    on_exit { hl += 0; if (x & (1 << 0)) return 0; if (True) {
    on_exit { hl += 1; if (x & (1 << 1)) return 1; if (True) {
    on_exit { hl += 2; if (x & (1 << 2)) return 2; if (True) {
    on_exit { hl += 3; if (x & (1 << 3)) return 3; if (True) {
    on_exit { hl += 4; if (x & (1 << 4)) return 4; if (True) {
    on_exit { hl += 5; if (x & (1 << 5)) return 5; if (True) {
    on_exit { hl += 6; if (x & (1 << 6)) return 6; if (True) {
    on_exit { hl += 7; if (x & (1 << 7)) return 7; if (True) {
    on_exit { hl += 8; if (x & (1 << 8)) return 8; if (True) {
    on_exit { hl += 9; if (x & (1 << 9)) return 9; if (True) {
    on_exit { hl += 10; if (x & (1 << 10)) return 10; if (True) {
    on_exit { hl += 11; if (x & (1 << 11)) return 11; if (True) {
    on_exit { hl += 12; if (x & (1 << 12)) return 12; if (True) {
    on_exit { hl += 13; if (x & (1 << 13)) return 13; if (True) {
    on_exit { hl += 14; if (x & (1 << 14)) return 14; if (True) {
    on_exit { hl += 15; if (x & (1 << 15)) return 15; if (True) {
    hl += "core";
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    return -1;
}

auto sub nested_noreturn() {
    on_exit { hl += 0; if (True) {
    on_exit { hl += 1; if (True) {
    on_exit { hl += 2; if (True) {
    on_exit { hl += 3; if (True) {
    on_exit { hl += 4; if (True) {
    on_exit { hl += 5; if (True) {
    on_exit { hl += 6; if (True) {
    on_exit { hl += 7; if (True) {
    on_exit { hl += 8; if (True) {
    on_exit { hl += 9; if (True) {
    on_exit { hl += 10; if (True) {
    on_exit { hl += 11; if (True) {
    on_exit { hl += 12; if (True) {
    on_exit { hl += 13; if (True) {
    on_exit { hl += 14; if (True) {
    on_exit { hl += 15; if (True) {
    hl += "core";
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    } }
    return -1;
}

auto sub nested_throw(int x, int y) {
    on_exit { hl += 0; if (x & (1 << 0)) return 0; if (True) {
    on_exit { hl += 1; if (x & (1 << 1)) return 1; if (True) {
    on_exit { hl += 2; if (x & (1 << 2)) return 2; if (True) {
    on_exit { hl += 3; if (x & (1 << 3)) return 3; if (True) {
    on_exit { hl += 4; if (x & (1 << 4)) return 4; if (True) {
    on_exit { hl += 5; if (x & (1 << 5)) return 5; if (True) {
    on_exit { hl += 6; if (x & (1 << 6)) return 6; if (True) {
    on_exit { hl += 7; if (x & (1 << 7)) return 7; if (True) {
    on_exit { hl += 8; if (x & (1 << 8)) return 8; if (True) {
    on_exit { hl += 9; if (x & (1 << 9)) return 9; if (True) {
    on_exit { hl += 10; if (x & (1 << 10)) return 10; if (True) {
    on_exit { hl += 11; if (x & (1 << 11)) return 11; if (True) {
    hl += "core";
    if (x & (1 << 29)) throw "CORE-ERROR";
    } } if (y == 11) throw "LEVEL-11";
    } }
    } }
    } }
    } } if (y == 7) throw "LEVEL-7";
    } }
    } }
    } }
    } }
    } }
    } }
    } } if (y == 0) throw "LEVEL-0";
    return -1;
}

sub show(string label, code c) {
    hl = ();
    auto rv;
    try {
        rv = c();
    } catch (hash<ExceptionInfo> ex) {
        rv = ex.err;
    }
    printf("%s: %y %s\n", label, rv, (map sprintf("%s", $1), hl).join(","));
}

show("noreturn", \nested_noreturn());
foreach int x in ((0, 1 << 0, 1 << 3, (1 << 5) | (1 << 9), 1 << 12)) {
    show(sprintf("return 0x%x", x), auto sub () { return nested_return(x); });
    foreach int y in ((-1, 0, 7, 11)) {
        show(sprintf("throw 0x%x %d", x, y), auto sub () { return nested_throw(x, y); });
        show(sprintf("throw 0x%x %d", x | (1 << 29), y), auto sub () { return nested_throw(x | (1 << 29), y); });
    }
}
