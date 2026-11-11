%modern
# locals of handlers that only run on an exception, which are never lowered inline: a local that the on_error handler
# of a closure declares and references (qcc could not compile it: qore#5657), loops in handlers and the loop and
# statement variables of handlers (AOT gave wrong results: qore#5658); see on-block-exit-closure-locals.qtest

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

#! a closure whose on_error handler declares a local that it references
string sub closure_ref_error() {
    list<string> log;
    code c = sub () {
        on_error {
            int i = 1;
            reference<int> r = \i;
            r += 10;
            log += sprintf("error i=%d", i);
        }
        throw "Y";
    };
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + chain(ex);
    }
    return log.join("; ");
}

#! a loop in an on_exit handler whose body declares a captured local: each closure captures the value of its
#! iteration
string sub loop_in_handler() {
    list<string> log;
    code c = sub () {
        on_exit {
            list<code> cl;
            foreach int i in (xrange(3)) {
                int v = i * 10;
                cl += int sub () { return v; };
            }
            log += sprintf("loop %y", (map $1(), cl));
        }
        throw "L";
    };
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + chain(ex);
    }
    return log.join("; ");
}

#! a foreach variable that an on_error handler declares and a closure captures
string sub foreach_variable() {
    list<string> log;
    code c = sub () {
        on_error {
            foreach int i in (xrange(2)) {
                code d = int sub () { return i; };
                log += sprintf("foreach %d", d());
            }
            log += "after the loop";
        }
        throw "FE";
    };
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + chain(ex);
    }
    return log.join("; ");
}

#! a for variable, a switch-block local and a while-body local that an on_exit handler declares and closures capture
string sub statement_variables() {
    list<string> log;
    code c = sub () {
        on_exit {
            list<code> cl;
            for (int i = 0; i < 2; ++i) {
                cl += int sub () { return i; };
            }
            log += sprintf("for %y", (map $1(), cl));
            switch (1) {
                case 1:
                    string sw = "switch";
                    code d = string sub () { return sw; };
                    log += d();
                    break;
            }
            int n = 0;
            while (n < 2) {
                int w = n * 3;
                code d = int sub () { return w; };
                log += sprintf("while %d", d());
                ++n;
            }
        }
        throw "SV";
    };
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + chain(ex);
    }
    return log.join("; ");
}

#! qore#5658: a loop in an on_exit handler that runs on an exception appends to a typed list that it declares
string sub loop_typed_list() {
    list<string> log;
    code c = sub () {
        on_exit {
            list<int> cl;
            foreach int i in (xrange(3)) {
                cl += i;
            }
            log += sprintf("exit %y", cl);
        }
        throw "TL";
    };
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + chain(ex);
    }
    return log.join("; ");
}

#! qore#5658: two handlers with loops, on the normal path and on an exception
string sub two_handlers(bool fail) {
    list<string> log;
    code c = sub () {
        on_exit {
            list<int> cl;
            foreach int i in (xrange(3)) {
                cl += i;
            }
            log += sprintf("exit %y", cl);
        }
        on_error {
            list<int> el;
            for (int i = 0; i < 2; ++i) {
                el += i;
            }
            log += sprintf("error %y", el);
        }
        if (fail) {
            throw "TH";
        }
    };
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + chain(ex);
    }
    return log.join("; ");
}

sub main() {
    # runs more than once so that the functions are compiled with tiered execution
    for (int i = 0; i < 3; ++i) {
        printf("closure ref error: %s\n", closure_ref_error());
        printf("loop in handler: %s\n", loop_in_handler());
        printf("foreach variable: %s\n", foreach_variable());
        printf("statement variables: %s\n", statement_variables());
        printf("loop typed list: %s\n", loop_typed_list());
        printf("two handlers: %s\n", two_handlers(False));
        printf("two handlers: %s\n", two_handlers(True));
    }
}

main();
