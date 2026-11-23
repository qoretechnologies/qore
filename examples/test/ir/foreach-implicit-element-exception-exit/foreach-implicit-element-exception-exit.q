%modern
%disable-warning unreferenced-variable
# the code that reads the thread's $# after a call that an exception from the body of a foreach in the callee left
# sees the $# of the caller; see foreach-implicit-element-exception-exit.qtest (qore#7379)
# Copyright (C) 2026 Qore Technologies, s.r.o.

list<string> log;

const Data = {"a": (0, 1, 2, 3, 4, 5, 6, 7, 8, 9)};
const Src = (10, 11, 12, 13, 14, 15, 16, 17);

sub thrower() {
    throw "X";
}

sub nop() {
}

int sub id(int i) {
    return i;
}

# throws from a call in the body of a foreach whose body reads $#
sub list_call_reads() {
    foreach auto a in (Src) {
        if ($# == 5) {
            thrower();
        }
    }
}

# throws from a call in the body of a foreach whose body does not read $#
sub list_call_no_read() {
    foreach auto a in (Src) {
        if (a == 15) {
            thrower();
        }
    }
}

# throws directly in a body that also makes a call
sub list_direct_with_call() {
    foreach auto a in (Src) {
        nop();
        if ($# == 5) {
            throw "X";
        }
    }
}

# throws directly in a body without a call
sub list_direct() {
    foreach auto a in (Src) {
        if ($# == 5) {
            throw "X";
        }
    }
}

# throws from the inner one of two nested loops
sub nested_call() {
    foreach auto a in (Src) {
        if ($# == 4) {
            foreach auto b in (Src) {
                if (id($#) == 6) {
                    thrower();
                }
            }
        }
    }
}

# throws from a foreach over a reference
sub ref_call() {
    list<auto> l = Src;
    foreach auto a in (\l) {
        if ($# == 5) {
            thrower();
        }
    }
}

# throws from a foreach over a lazy pipeline (the fused lowering)
sub fused_call() {
    foreach auto a in (select Src, $1 > 0) {
        if ($# == 5) {
            thrower();
        }
    }
}

# throws from a map whose expression calls a function in a foreach body
sub map_call() {
    foreach auto a in (Src) {
        if ($# == 3) {
            list<auto> l = map ($# == 5 ? thrower() : $1), Src;
        }
    }
}

# throws from a select in a foreach body
sub select_call() {
    foreach auto a in (Src) {
        if ($# == 3) {
            list<auto> l = select Src, ($# == 5 ? thrower() : True);
        }
    }
}

# throws from a hash map in a foreach body
sub hash_map_call() {
    foreach auto a in (Src) {
        if ($# == 3) {
            hash<auto> h = map {$1.toString(): $# == 5 ? thrower() : $1}, Src;
        }
    }
}

# catches the exception in the body and rethrows it
sub rethrow_in_body() {
    foreach auto a in (Src) {
        try {
            if ($# == 5) {
                thrower();
            }
        } catch (hash<ExceptionInfo> ex) {
            rethrow;
        }
    }
}

# catches the exception in the body: the loop goes on and ends normally
sub caught_in_body() {
    foreach auto a in (Src) {
        try {
            if ($# == 5) {
                thrower();
            }
        } catch () {
        }
    }
}

# catches the exception around the loop in the callee, then throws again outside of it
sub caught_around_loop() {
    try {
        foreach auto a in (Src) {
            if ($# == 5) {
                thrower();
            }
        }
    } catch () {
    }
    thrower();
}

# an on_error handler in the body, which runs on the way out
sub on_error_in_body() {
    foreach auto a in (Src) {
        on_error nop();
        if ($# == 5) {
            thrower();
        }
    }
}

# three levels of calls with foreach loops: the innermost throws
sub level3() {
    foreach auto a in (Src) {
        if ($# == 7) {
            thrower();
        }
    }
}

sub level2() {
    foreach auto a in (Src) {
        if ($# == 6) {
            level3();
        }
    }
}

sub level1() {
    foreach auto a in (Src) {
        if ($# == 5) {
            level2();
        }
    }
}

# the caller: the thread's $# after the exception is read by a find expression, a context statement and the catch
# block, in a loop of the caller
sub check(string label, code c) {
    list<string> out = ();
    foreach auto o in ((20, 21, 22)) {
        *int caught;
        try {
            c();
        } catch () {
            caught = (find %a in Data where (%a == $#));
        }
        int found = (find %a in Data where (%a == $#));
        list<int> ctx = ();
        context (Data) where (%a == $#) {
            ctx += %a;
        }
        out += sprintf("%d:%y:%d:%y", $#, caught, found, ctx);
    }
    log += label + " " + (foldl $1 + " " + $2, out);
}

check("list call reads", \list_call_reads());
check("list call no read", \list_call_no_read());
check("list direct with call", \list_direct_with_call());
check("list direct", \list_direct());
check("nested call", \nested_call());
check("ref call", \ref_call());
check("fused call", \fused_call());
check("map call", \map_call());
check("select call", \select_call());
check("hash map call", \hash_map_call());
check("rethrow in body", \rethrow_in_body());
check("caught in body", \caught_in_body());
check("caught around loop", \caught_around_loop());
check("on_error in body", \on_error_in_body());
check("three levels", \level1());
# a closure in the caller's loop that calls the callee
check("closure", sub () { list_call_reads(); });

# the caller's foreach is itself in a callee of a loop: the outer caller's $# after the exception
sub outer_check() {
    foreach auto o in ((30, 31)) {
        try {
            foreach auto i in ((40, 41)) {
                list_call_reads();
            }
        } catch () {
        }
        log += sprintf("outer %d %d", $#, (find %a in Data where (%a == $#)));
    }
}
outer_check();

map printf("%s\n", $1), log;
