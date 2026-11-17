%modern
%disable-warning unreferenced-variable
# $# in a foreach over a reference or over a fused map, select or take pipeline nested in the body of another foreach
# is the inner loop's index; see foreach-nested-implicit-index.qtest (qore#6383)
# Copyright (C) 2026 Qore Technologies, s.r.o.

string sub fmt(auto v) { return sprintf("%y", v); }

list sub inner_map() { return map $#, (7, 8); }

class C {
    list m() {
        list r;
        foreach string x in (("x", "y")) {
            list m = (0, 0);
            foreach auto y in (\m) {
                y = $# * 10;
            }
            r += (m, $#);
        }
        return r;
    }
}

list sub cases() {
    list out;
    list r;

    foreach auto x in ((1, 2)) {
        list m = (0, 1);
        foreach auto y in (\m) {
            y = $#;
        }
        r += m;
    }
    out += "ref in foreach " + fmt(r);

    r = ();
    foreach int x in ((1, 2)) {
        list<int> m = (5, 5, 5);
        foreach int y in (\m) {
            y = $# + x * 10;
        }
        # the outer index again after the inner loop
        r += (m, $#);
    }
    out += "typed ref in foreach, outer after " + fmt(r);

    r = ();
    foreach auto x in ((1, 2)) {
        foreach auto y in (map $1 * 2, (5, 6)) {
            r += ($#, y);
        }
        r += $#;
    }
    out += "fused map in foreach " + fmt(r);

    r = ();
    foreach auto x in ((1, 2)) {
        foreach auto y in (select (5, 6, 7), $1 > 5) {
            r += $#;
        }
    }
    out += "fused select in foreach " + fmt(r);

    r = ();
    foreach auto x in ((1, 2)) {
        foreach auto y in ((take 2, (map $1, (5, 6, 7)))) {
            r += $#;
        }
    }
    out += "fused take in foreach " + fmt(r);

    r = ();
    list outer = (0, 0);
    foreach auto x in (\outer) {
        list m = (0, 0);
        foreach auto y in (\m) {
            y = $#;
        }
        x = $#;
        r += m;
    }
    out += "ref in ref " + fmt((r, outer));

    r = ();
    list outer2 = (0, 0);
    foreach auto x in (\outer2) {
        foreach auto y in (map $1, (5, 6)) {
            r += $#;
        }
        x = $# + 10;
    }
    out += "fused in ref " + fmt((r, outer2));

    r = ();
    foreach auto x in ((1, 2)) {
        list m = (0, 1, 2);
        foreach auto y in (\m) {
            if ($# == 1) {
                continue;
            }
            y = $# + 100;
            r += inner_map();
            if ($# == 2) {
                break;
            }
        }
        r += (m, $#);
    }
    out += "continue break call " + fmt(r);

    C c();
    out += "method " + fmt(c.m());

    code cl = list sub () {
        list rv;
        foreach auto x in ((1, 2)) {
            foreach auto y in (map $1, (5, 6)) {
                rv += $#;
            }
        }
        return rv;
    };
    out += "closure " + fmt(cl());
    return out;
}

foreach string line in (cases()) {
    printf("%s\n", line);
}

# an on_exit handler of an outer loop body runs with the outer index when a return leaves a loop nested in the body,
# and so does a catch block in the outer loop body when an exception leaves the nested loop: the nested loop has ended
# when they run
list log;

sub ref_in_ref_return() {
    list o = (7, 8);
    foreach auto x in (\o) {
        on_exit log += "ref/ref on_exit " + $#;
        list m = (1, 2, 3);
        foreach auto y in (\m) {
            if ($# == 1) {
                return;
            }
        }
    }
}
ref_in_ref_return();

sub ref_in_value_catch() {
    foreach auto x in ((7, 8)) {
        try {
            list m = (1, 2, 3);
            foreach auto y in (\m) {
                if ($# == 1) {
                    throw "X";
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log += "value/ref catch " + $# + " " + x;
        }
    }
}
ref_in_value_catch();

sub fused_in_value_catch() {
    foreach auto x in ((7, 8)) {
        try {
            foreach auto y in (select (1, 2, 3), $1 > 0) {
                if ($# == 1) {
                    throw "X";
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log += "value/fused catch " + $# + " " + x;
        }
    }
}
fused_in_value_catch();

sub value_in_value_return() {
    foreach auto x in ((7, 8)) {
        on_exit log += "value/value on_exit " + $#;
        foreach auto y in ((1, 2, 3)) {
            if ($# == 1) {
                return;
            }
        }
    }
}
value_in_value_return();

sub value_in_ref_return() {
    list o = (7, 8);
    foreach auto x in (\o) {
        on_exit log += "ref/value on_exit " + $#;
        foreach auto y in ((1, 2, 3)) {
            if ($# == 1) {
                return;
            }
        }
    }
}
value_in_ref_return();

sub fused_in_ref_return() {
    list o = (7, 8);
    foreach auto x in (\o) {
        on_exit log += "ref/fused on_exit " + $#;
        foreach auto y in (map $1, (1, 2, 3)) {
            if ($# == 1) {
                return;
            }
        }
    }
}
fused_in_ref_return();

sub inner_on_exit() {
    list r;
    foreach auto x in ((7, 8)) {
        list m = (1, 2);
        foreach auto y in (\m) {
            on_exit r += $#;
        }
    }
    log += "inner on_exit " + sprintf("%y", r);
}
inner_on_exit();

foreach string line in (log) {
    printf("%s\n", line);
}
