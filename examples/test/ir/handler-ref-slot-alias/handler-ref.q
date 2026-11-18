%modern
%allow-weak-references
# references to locals that are assigned after an on_block_exit handler ran out of line (on an exception) in the same
# function; see handler-ref-slot-alias.qtest

#! a reference taken after an on_error handler ran in a try block
string sub after_try() {
    list<string> log;
    try {
        on_error {
            log += "inner";
        }
        throw "IN";
    } catch (hash<ExceptionInfo> ex) {
        log += "inner caught " + ex.err;
    }
    int o = 2;
    int p = 3;
    reference<int> ro = \o;
    ro *= 5;
    log += sprintf("o=%d p=%d", o, p);
    return log.join("; ");
}

#! the same in a loop: the reference is declared in the try block of each iteration
string sub in_loop() {
    list<string> log;
    for (int n = 0; n < 2; ++n) {
        try {
            on_error {
                log += sprintf("error %d", n);
            }
            int m = 7;
            reference<int> rm = \m;
            rm += n;
            log += sprintf("m=%d", m);
            throw "BS";
        } catch (hash<ExceptionInfo> ex) {
            log += "caught " + ex.err;
        }
    }
    return log.join("; ");
}

#! a reference taken in a handler after a nested on_error handler ran
string sub in_handler() {
    list<string> log;
    code c = sub () {
        on_error {
            try {
                on_error {
                    log += "nested";
                }
                throw "IN";
            } catch (hash<ExceptionInfo> ex) {
                log += "nested caught " + ex.err;
            }
            int o = 2;
            reference<int> ro = \o;
            ro *= 3;
            log += sprintf("outer o=%d", o);
        }
        throw "X";
    };
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + ex.err;
    }
    return log.join("; ");
}

#! a local declared after the handler and assigned NOTHING explicitly
string sub nothing_assigned() {
    list<string> log;
    try {
        on_error {
            log += "inner";
        }
        throw "IN";
    } catch () {
    }
    *int x = NOTHING;
    reference<*int> rx = \x;
    rx = 4;
    log += sprintf("x=%y", x);
    return log.join("; ");
}

#! a reference local declared without a value, bound by an assignment that raises in the second iteration, and assigned
#! through after a handler ran (the review's p17)
string sub bound_then_failed() {
    list<string> log;
    *reference<int> r;
    int o = 1;
    for (int i = 0; i < 2; ++i) {
        try {
            r = \o;
        } catch (hash<ExceptionInfo> ex) {
            log += sprintf("bind %d %s", i, ex.err);
        }
    }
    try {
        on_error {
            log += sprintf("handler o=%d", o);
        }
        throw "IN";
    } catch () {
    }
    r = 5;
    log += sprintf("o=%d", o);
    return log.join("; ");
}

auto sub noth() {
    return NOTHING;
}

#! a handler assigns NOTHING to a reference local that has no value, then the code binds it (the review's p14a)
string sub handler_assigns_nothing() {
    list<string> log;
    *reference<int> r;
    int o = 1;
    try {
        on_error {
            r = noth();
        }
        throw "IN";
    } catch () {
    }
    try {
        r = \o;
    } catch (hash<ExceptionInfo> ex) {
        log += "bind " + ex.err;
    }
    r = 5;
    log += sprintf("o=%d", o);
    return log.join("; ");
}

#! a reference local declared without a value before the handler and bound by its first assignment after it
string sub declared_before_bound_after() {
    list<string> log;
    *reference<int> r;
    int o = 2;
    try {
        on_error {
            log += "handler";
        }
        throw "IN";
    } catch () {
    }
    r = \o;
    r += 3;
    log += sprintf("o=%d", o);
    return log.join("; ");
}

#! a reference local declared without a value after the handler and bound by its first assignment
string sub declared_after_bound() {
    list<string> log;
    int o = 2;
    try {
        on_error {
            log += "handler";
        }
        throw "IN";
    } catch () {
    }
    *reference<int> r;
    r = \o;
    r *= 4;
    log += sprintf("o=%d", o);
    return log.join("; ");
}

#! a reference local declared after the handler in a loop body, after an earlier iteration bound it
string sub loop_rebound() {
    list<string> log;
    list<int> vals = (1, 2, 3);
    for (int i = 0; i < 3; ++i) {
        try {
            on_error {
                log += sprintf("handler %d", i);
            }
            if (i) {
                throw "IN";
            }
        } catch () {
        }
        reference<int> r = \vals[i];
        r += 10;
    }
    log += sprintf("vals=%y", vals);
    return log.join("; ");
}

#! the same in a recursive function
string sub recursive(int depth) {
    try {
        on_error {
            depth += 0;
        }
        throw "IN";
    } catch () {
    }
    int o = depth;
    reference<int> r = \o;
    r += 100;
    return depth ? sprintf("%d,%s", o, recursive(depth - 1)) : string(o);
}

#! the same in a method
class Holder {
    string run() {
        list<string> log;
        try {
            on_error {
                log += "handler";
            }
            throw "IN";
        } catch () {
        }
        int o = 6;
        reference<int> r = \o;
        r -= 1;
        log += sprintf("o=%d", o);
        return log.join("; ");
    }
}

class A1 {
    string n() {
        return "A1";
    }
}

class B1 {
    string n() {
        return "B1";
    }
}

auto sub wrong() {
    return new B1();
}

auto sub str() {
    return "x";
}

#! assignments to locals that are not references raise; the handlers that run afterwards see the values before them
string sub failed_assignments() {
    list<string> log;
    int x = 1;
    try {
        x = noth();
    } catch (hash<ExceptionInfo> ex) {
        log += "x " + ex.err;
    }
    A1 a = new A1();
    try {
        a = wrong();
    } catch (hash<ExceptionInfo> ex) {
        log += "a " + ex.err;
    }
    softint si = 3;
    try {
        si = wrong();
    } catch (hash<ExceptionInfo> ex) {
        log += "si " + ex.err;
    }
    list<int> l = (1, 2);
    try {
        l = str();
    } catch (hash<ExceptionInfo> ex) {
        log += "l " + ex.err;
    }
    A1 w;
    try {
        w := str();
    } catch (hash<ExceptionInfo> ex) {
        log += "w " + ex.err;
    }
    try {
        on_error {
            log += sprintf("handler x=%y a=%s si=%y l=%y w=%y", x, a.n(), si, l, exists w);
        }
        throw "IN";
    } catch () {
    }
    log += sprintf("x=%y a=%s si=%y l=%y", x, a.n(), si, l);
    return log.join("; ");
}

sub main() {
    # every case runs more than once, so that the functions are compiled with tiered execution
    for (int i = 0; i < 3; ++i) {
        printf("after try: %s\n", after_try());
        printf("in loop: %s\n", in_loop());
        printf("in handler: %s\n", in_handler());
        printf("nothing assigned: %s\n", nothing_assigned());
        printf("bound then failed: %s\n", bound_then_failed());
        printf("handler assigns nothing: %s\n", handler_assigns_nothing());
        printf("declared before bound after: %s\n", declared_before_bound_after());
        printf("declared after bound: %s\n", declared_after_bound());
        printf("loop rebound: %s\n", loop_rebound());
        printf("recursive: %s\n", recursive(2));
        printf("method: %s\n", (new Holder()).run());
        printf("failed assignments: %s\n", failed_assignments());
    }
}

main();

# the same at top level
{
    list<string> log;
    try {
        on_error {
            log += "handler";
        }
        throw "IN";
    } catch () {
    }
    int o = 8;
    reference<int> r = \o;
    r += 1;
    log += sprintf("o=%d", o);
    printf("top level: %s\n", log.join("; "));
}
