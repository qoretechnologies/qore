#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# a pipeline with a select stage whose predicate is a constant (#6014) gives the value of a return that a handler
# interrupts (#6028); see fold-select-constant-return.qtest

%modern
%enable-all-warnings

our list<string> log = ();
our int live = 0;

class T {
    private {
        string n;
    }
    constructor(string n) {
        self.n = n;
        ++live;
    }
    destructor() {
        log += "~" + n;
        --live;
    }
}

T sub mk(string n) {
    return new T(n);
}

# a fold over a map stage before the select, in an on_exit handler that throws behind a constant condition
auto sub fold_map_false() {
    T z("z");
    if (True) {
        on_exit {
            log += "x";
            if (True) {
                throw "E";
            }
        }
        return foldl $1 + $2, (select (map (mk("m" + $1),), (1, 2)), False);
    }
}
auto sub fold_map_true() {
    T z("z");
    if (True) {
        on_exit {
            log += "x";
            if (True) {
                throw "E";
            }
        }
        return foldl $1 + $2, (select (map (mk("m" + $1),), (1, 2)), True);
    }
}

# a fold over a map stage after the select, in an on_success handler
auto sub fold_select_map_false() {
    T z("z");
    if (True) {
        on_success {
            log += "s";
            if (True) {
                throw "E";
            }
        }
        return foldl $1 + $2, (map (mk("m" + $1),), (select (1, 2), False));
    }
}
auto sub fold_select_map_true() {
    T z("z");
    if (True) {
        on_success {
            log += "s";
            if (True) {
                throw "E";
            }
        }
        return foldl $1 + $2, (map (mk("m" + $1),), (select (1, 2), True));
    }
}

# a list over an iterator, the map stage after the select
auto sub list_iterator_false() {
    T z("z");
    if (True) {
        on_exit {
            log += "x";
            if (True) {
                throw "E";
            }
        }
        return map mk("m" + $1), (select new ListIterator((1, 2)), False);
    }
}
auto sub list_iterator_true() {
    T z("z");
    if (True) {
        on_exit {
            log += "x";
            if (True) {
                throw "E";
            }
        }
        return map mk("m" + $1), (select new ListIterator((1, 2)), True);
    }
}

# a list, the map stage before the select, in a try block of the function whose handler catches the exception
auto sub list_try_false() {
    T z("z");
    try {
        on_exit {
            log += "x";
            if (True) {
                throw "E";
            }
        }
        return select (map mk("m" + $1), (1, 2)), False;
    } catch (hash<ExceptionInfo> ex) {
        log += "catch:" + ex.err;
    }
    return "after";
}
auto sub list_try_true() {
    T z("z");
    try {
        on_exit {
            log += "x";
            if (True) {
                throw "E";
            }
        }
        return select (map mk("m" + $1), (1, 2)), True;
    } catch (hash<ExceptionInfo> ex) {
        log += "catch:" + ex.err;
    }
    return "after";
}

# the functions are called directly, so that they run as native code in the jit and tiered modes, and each three times
auto sub call(string fn) {
    switch (fn) {
        case "fold map false": return fold_map_false();
        case "fold map true": return fold_map_true();
        case "fold select map false": return fold_select_map_false();
        case "fold select map true": return fold_select_map_true();
        case "list iterator false": return list_iterator_false();
        case "list iterator true": return list_iterator_true();
        case "list try false": return list_try_false();
        case "list try true": return list_try_true();
    }
}

foreach string fn in (("fold map false", "fold map true", "fold select map false", "fold select map true",
        "list iterator false", "list iterator true", "list try false", "list try true")) {
    for (int i = 0; i < 3; ++i) {
        log = ();
        try {
            auto v = call(fn);
            log += "returned:" + (v.typeCode() == NT_LIST ? sprintf("list(%d)", v.size()) : v.toString());
        } catch (hash<ExceptionInfo> ex) {
            log += "caught:" + ex.err;
        }
        printf("%s: %y live=%d\n", fn, log, live);
    }
}
