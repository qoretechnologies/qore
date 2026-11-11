%modern
# the locals that on_block_exit handlers declare and that are closure-bound (referenced or captured by a closure),
# when the handler runs on an exception and on the normal exit path; see on-block-exit-closure-locals.qtest (handlers
# that only run on an exception are in handler-modes.q)

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

#! a closure whose on_exit handler declares a local that it references
string sub closure_ref_exit(bool fail) {
    list<string> log;
    code c = sub () {
        on_exit {
            list<int> l = (1, 2);
            if (fail) {
                log += sprintf("exit l=%y", l);
            } else {
                reference<list<int>> r = \l;
                r += 3;
                log += sprintf("exit ref l=%y", l);
            }
        }
        if (fail) {
            throw "X";
        }
    };
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + chain(ex);
    }
    return log.join("; ");
}

#! a closure whose on_exit handler declares a local that a closure captures
string sub closure_capture_exit(bool fail) {
    list<string> log;
    code c = sub () {
        on_exit {
            string s = fail ? "on exception" : "normal";
            code d = string sub () { return "inner " + s; };
            log += d();
        }
        if (fail) {
            throw "Z";
        }
    };
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + chain(ex);
    }
    return log.join("; ");
}

#! a function whose on_exit handler declares a local that a closure captures
string sub function_capture_exit(reference<list<string>> log, bool fail) {
    on_exit {
        int n = 7;
        code d = int sub () { return n * 2; };
        log += sprintf("function exit %d", d());
    }
    if (fail) {
        throw "F";
    }
    return "returned";
}

#! a nested on_exit handler in an on_error handler, each declaring a captured local
string sub nested_handlers() {
    list<string> log;
    code c = sub () {
        on_error {
            string outer = "outer";
            on_exit {
                string inner = "inner";
                code d = string sub () { return outer + "/" + inner; };
                log += d();
            }
            code e = string sub () { return outer; };
            log += "error " + e();
        }
        throw "N";
    };
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + chain(ex);
    }
    return log.join("; ");
}

#! a handler of a recursive closure: each frame's handler has its own captured local
string sub recursive_handler() {
    list<string> log;
    code c;
    c = sub (int depth) {
        on_exit {
            int d = depth;
            code f = int sub () { return d; };
            log += sprintf("depth %d", f());
        }
        if (depth < 2) {
            c(depth + 1);
        } else {
            throw "R";
        }
    };
    try {
        c(0);
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + chain(ex);
    }
    return log.join("; ");
}

#! an on_success handler with a captured local, and the parent's captured local read by an on_exit handler
string sub success_and_parent(bool fail) {
    list<string> log;
    code c = sub () {
        string p = "parent";
        code keep = string sub () { return p; };
        on_success {
            string s = "success";
            code d = string sub () { return s + "+" + keep(); };
            log += d();
        }
        on_exit {
            log += "exit " + p;
        }
        if (fail) {
            throw "S";
        }
    };
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + chain(ex);
    }
    return log.join("; ");
}

#! a catch variable that an on_error handler declares and a closure captures
string sub catch_variable() {
    list<string> log;
    code c = sub () {
        on_error {
            try {
                throw "INNER";
            } catch (hash<ExceptionInfo> e) {
                code d = string sub () { return e.err; };
                log += "catch variable " + d();
            }
            log += "after the try";
        }
        throw "M";
    };
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + chain(ex);
    }
    return log.join("; ");
}

#! thread_exit in a closure whose on_exit handler declares a captured local, in a background thread
string sub thread_exit_closure() {
    Queue q();
    code c = sub () {
        on_exit {
            string s = "closure";
            code d = string sub () { return s; };
            q.push("on_exit " + d());
        }
        thread_exit;
    };
    background sub () {
        on_exit {
            q.push("END");
        }
        c();
        q.push("not reached");
    }();
    list<string> l;
    while (True) {
        string v = q.get(60s);
        if (v == "END") {
            break;
        }
        l += v;
    }
    return l.join("; ");
}

#! thread_exit in a function run as a background thread, whose on_exit handler declares a captured local
sub thread_exit_function(Queue q) {
    on_exit {
        string s = "function";
        code d = string sub () { return s; };
        q.push("on_exit " + d());
        q.push("END");
    }
    thread_exit;
}

string sub thread_exit_function_run() {
    Queue q();
    background thread_exit_function(q);
    list<string> l;
    while (True) {
        string v = q.get(60s);
        if (v == "END") {
            break;
        }
        l += v;
    }
    return l.join("; ");
}

sub main() {
    # every case runs more than once, on both paths where it has two, so that the functions are compiled with tiered
    # execution, and the normal path (where the handler is lowered inline) runs before and after the exception path
    for (int i = 0; i < 3; ++i) {
        printf("closure ref exit: %s\n", closure_ref_exit(False));
        printf("closure ref exit: %s\n", closure_ref_exit(True));
        printf("closure capture exit: %s\n", closure_capture_exit(False));
        printf("closure capture exit: %s\n", closure_capture_exit(True));
        list<string> log;
        printf("function capture exit: %s; %s\n", function_capture_exit(\log, False), log.join("; "));
        log = ();
        try {
            function_capture_exit(\log, True);
        } catch (hash<ExceptionInfo> ex) {
            log += "caught " + chain(ex);
        }
        printf("function capture exit: %s\n", log.join("; "));
        printf("nested handlers: %s\n", nested_handlers());
        printf("recursive handler: %s\n", recursive_handler());
        printf("success and parent: %s\n", success_and_parent(False));
        printf("success and parent: %s\n", success_and_parent(True));
        printf("catch variable: %s\n", catch_variable());
        printf("thread_exit in a closure: %s\n", thread_exit_closure());
        printf("thread_exit in a thread function: %s\n", thread_exit_function_run());
    }
}

main();
