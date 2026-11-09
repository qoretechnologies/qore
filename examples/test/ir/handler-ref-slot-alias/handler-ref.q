%modern
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

sub main() {
    # every case runs more than once, so that the functions are compiled with tiered execution
    for (int i = 0; i < 3; ++i) {
        printf("after try: %s\n", after_try());
        printf("in loop: %s\n", in_loop());
        printf("in handler: %s\n", in_handler());
        printf("nothing assigned: %s\n", nothing_assigned());
    }
}

main();
