%modern
# on_block_exit handlers of closures that read and update the closure-bound locals of the closure they belong to
# (referenced with \var or captured by a closure), when the handler runs on an exception (out of line, from its own IR
# function) and on the normal exit path; see on-block-exit-closure-locals.qtest, which also runs these closures from
# their serialized IR in AOT executables

#! describes an exception and the exceptions chained to it, which must be only those that the code raised
string sub chain(hash<ExceptionInfo> ex) {
    list<string> l;
    *hash<ExceptionInfo> e = ex;
    while (e) {
        l += e.err == "LVALUE-ERROR" ? sprintf("%s(%s)", e.err, e.desc) : e.err;
        e = e.next;
    }
    return l.join("+");
}

#! adds 30 to the variable it is passed a reference to
sub add30(reference<int> r) {
    r += 30;
}

#! runs a closure and returns its log, with the exception it raised
string sub run(code c, bool fail, reference<list<string>> log) {
    try {
        c(fail);
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + chain(ex);
    }
    return log.join("; ");
}

#! an on_error handler passes a reference to a local of the closure to a function
string sub error_ref() {
    list<string> log;
    code c = sub (bool fail) {
        int e = 1;
        on_error {
            add30(\e);
            log += sprintf("error e=%d", e);
        }
        if (fail) {
            throw "ER";
        }
    };
    return run(c, True, \log);
}

#! an on_error handler calls a closure that it creates, which updates a local of the closure the handler belongs to
string sub error_closure() {
    list<string> log;
    code c = sub (bool fail) {
        int e = 1;
        on_error {
            code inc = sub () { e += 30; };
            inc();
            log += sprintf("error e=%d", e);
        }
        if (fail) {
            throw "EC";
        }
    };
    return run(c, True, \log);
}

#! an on_error handler starts a thread that reads a local of the closure the handler belongs to
string sub error_background() {
    list<string> log;
    code c = sub (bool fail) {
        int e = 31;
        on_error {
            Queue q();
            background sub () {
                try {
                    int seen = e;
                    q.push(sprintf("background e=%d", seen));
                } catch (hash<ExceptionInfo> ex) {
                    q.push("background " + chain(ex));
                }
            }();
            log += q.get();
        }
        if (fail) {
            throw "EB";
        }
    };
    return run(c, True, \log);
}

#! an on_exit handler passes a reference to a local of the closure to a function
string sub exit_ref(bool fail) {
    list<string> log;
    code c = sub (bool fail) {
        int e = 1;
        on_exit {
            add30(\e);
            log += sprintf("exit e=%d", e);
        }
        if (fail) {
            throw "XR";
        }
    };
    return run(c, fail, \log);
}

#! an on_success handler passes a reference to a local of the closure to a function; it does not run on an exception
string sub success_ref(bool fail) {
    list<string> log;
    code c = sub (bool fail) {
        int e = 1;
        on_success {
            add30(\e);
            log += sprintf("success e=%d", e);
        }
        if (fail) {
            throw "SR";
        }
    };
    return run(c, fail, \log);
}

#! the handlers of a block in a closure update a local of the block and a local of the closure
string sub block_handlers(bool fail) {
    list<string> log;
    code c = sub (bool fail) {
        int e = 1;
        {
            int b = 2;
            on_exit {
                add30(\b);
                add30(\e);
                log += sprintf("block exit b=%d e=%d", b, e);
            }
            on_error {
                code inc = sub () { b += 100; e += 100; };
                inc();
                log += sprintf("block error b=%d e=%d", b, e);
            }
            if (fail) {
                throw "BH";
            }
        }
        log += sprintf("after the block e=%d", e);
    };
    return run(c, fail, \log);
}

#! an on_exit handler declares a closure-bound local of its own and also updates a local of the closure
string sub own_and_parent(bool fail) {
    list<string> log;
    code c = sub (bool fail) {
        int e = 1;
        on_exit {
            int h = 5;
            add30(\h);
            add30(\e);
            code d = int sub () { return h + e; };
            log += sprintf("exit h=%d e=%d sum=%d", h, e, d());
        }
        if (fail) {
            throw "OP";
        }
    };
    return run(c, fail, \log);
}

sub main() {
    # every case runs more than once, on both paths where it has two, so that the functions are compiled with tiered
    # execution, and the normal path (where the handler is lowered inline) runs before and after the exception path
    for (int i = 0; i < 3; ++i) {
        printf("error ref: %s\n", error_ref());
        printf("error closure: %s\n", error_closure());
        printf("error background: %s\n", error_background());
        printf("exit ref: %s\n", exit_ref(False));
        printf("exit ref: %s\n", exit_ref(True));
        printf("success ref: %s\n", success_ref(False));
        printf("success ref: %s\n", success_ref(True));
        printf("block handlers: %s\n", block_handlers(False));
        printf("block handlers: %s\n", block_handlers(True));
        printf("own and parent: %s\n", own_and_parent(False));
        printf("own and parent: %s\n", own_and_parent(True));
    }
}

main();
