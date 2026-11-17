%modern
%disable-warning unreferenced-variable
# the handlers of the enclosing blocks see the $# of the code around a foreach that an exception leaves; see
# foreach-implicit-element-restore.qtest (qore#6432)
# Copyright (C) 2026 Qore Technologies, s.r.o.

list log;

# qore#6432: the handlers of the enclosing blocks run with the element of the code around the loop that the exception
# leaves
int sub id(int i) { return i; }

sub ref_in_ref_throw() {
    list o = (7, 8);
    foreach auto x in (\o) {
        on_error log += "ref/ref on_error " + $# + " " + x;
        list m = (1, 2, 3);
        foreach auto y in (\m) {
            if ($# == 1) {
                throw "X";
            }
        }
    }
}
try { ref_in_ref_throw(); } catch () {}

sub ref_in_value_throw() {
    foreach auto x in ((7, 8)) {
        on_error log += "value/ref on_error " + $# + " " + x;
        list m = (1, 2, 3);
        foreach auto y in (\m) {
            if ($# == 1) {
                throw "X";
            }
        }
    }
}
try { ref_in_value_throw(); } catch () {}

sub fused_in_value_throw() {
    foreach auto x in ((7, 8)) {
        on_error log += "value/fused on_error " + $# + " " + x;
        foreach auto y in (select (1, 2, 3), $1 > 0) {
            if ($# == 1) {
                throw "X";
            }
        }
    }
}
try { fused_in_value_throw(); } catch () {}

# the inner loop calls a function, so it keeps its element at run time
sub value_in_value_throw() {
    foreach auto x in ((7, 8)) {
        on_error log += "value/value on_error " + $# + " " + x;
        foreach auto y in ((1, 2, 3)) {
            if (id($#) == 1) {
                throw "X";
            }
        }
    }
}
try { value_in_value_throw(); } catch () {}

sub value_in_ref_throw() {
    list o = (7, 8);
    foreach auto x in (\o) {
        on_error log += "ref/value on_error " + $# + " " + x;
        foreach auto y in ((1, 2, 3)) {
            if (id($#) == 1) {
                throw "X";
            }
        }
    }
}
try { value_in_ref_throw(); } catch () {}

# on_exit with an exception, an on_error of a block in the outer body, and a handler of the function's block
sub on_exit_throw() {
    foreach auto x in ((7, 8)) {
        on_exit log += "on_exit throw " + $# + " " + x;
        list m = (1, 2);
        foreach auto y in (\m) {
            if ($# == 1) {
                throw "X";
            }
        }
    }
}
try { on_exit_throw(); } catch () {}

sub block_on_error_throw() {
    foreach auto x in ((7, 8)) {
        {
            on_error log += "block on_error " + $# + " " + x;
            foreach auto y in (map $1, (1, 2)) {
                if ($# == 1) {
                    throw "X";
                }
            }
        }
    }
}
try { block_on_error_throw(); } catch () {}

sub inner_handler_throw() {
    list o = (7, 8);
    foreach auto x in (\o) {
        on_error log += "outer on_error " + $#;
        list m = (1, 2);
        foreach auto y in (\m) {
            on_error log += "inner on_error " + $#;
            if ($# == 1) {
                throw "X";
            }
        }
    }
}
try { inner_handler_throw(); } catch () {}

# a catch block in the outer body, then the loop goes on
sub catch_continue() {
    list r;
    foreach auto x in ((7, 8)) {
        try {
            list m = (1, 2, 3);
            foreach auto y in (\m) {
                if (id($#) == 1) {
                    throw "X";
                }
            }
        } catch () {
            r += $#;
        }
        r += $#;
    }
    log += "catch continue " + sprintf("%y", r);
}
catch_continue();

# three levels, with a fused outer loop
sub three_levels() {
    foreach auto x in (map $1, (7, 8)) {
        on_error log += "three levels on_error " + $# + " " + x;
        list m = (1, 2);
        foreach auto y in (\m) {
            foreach auto z in ((1, 2, 3)) {
                if (id($#) == 2) {
                    throw "X";
                }
            }
        }
    }
}
try { three_levels(); } catch () {}

# the assignment of the loop variable raises before the inner loop sets its element
sub assign_throw() {
    foreach auto x in ((7, 8)) {
        on_error log += "assign on_error " + $# + " " + x;
        list l = (1, "a");
        foreach int y in (l) {
        }
    }
}
try { assign_throw(); } catch () {}

sub fused_assign_throw() {
    foreach auto x in ((7, 8)) {
        on_error log += "fused assign on_error " + $# + " " + x;
        foreach int y in (map $1, (1, "a")) {
        }
    }
}
try { fused_assign_throw(); } catch () {}

# a catch block in a foreach body whose statements raise is left, and its variable released, before the handler of
# the function's block runs
class D {
    public {
        string n;
    }
    constructor(string n) {
        self.n = n;
    }
    destructor() {
        log += "D " + n + " destroyed";
    }
}

sub thrower() {
    throw "B";
}

sub catch_var_release() {
    on_error log += "catch var on_error";
    foreach auto x in ((1, 2)) {
        try {
            throw "A", "", new D("a");
        } catch (hash<ExceptionInfo> ex) {
            thrower();
        }
    }
}
try { catch_var_release(); } catch () {}

foreach string line in (log) {
    printf("%s\n", line);
}
