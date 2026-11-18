%modern
# foreach statements over pipelines that IR fuses into the loop, whose stage expressions raise an exception: the
# element, the iterator, the source and the loop variable are released before the handler runs, as the AST
# interpreter unwinds the foreach statement; see fused-foreach-stage-exception.qtest

class Holder {
    public {
        Queue log;
        string n;
    }

    constructor(Queue l, string nn) {
        log = l;
        n = nn;
    }

    destructor() {
        log.push("released " + n);
    }
}

Holder sub fail_on(Holder h, string name) {
    if (h.n == name) {
        throw "STAGE", h.n;
    }
    return h;
}

bool sub select_fail_on(Holder h, string name) {
    if (h.n == name) {
        throw "SELECT", h.n;
    }
    return True;
}

list<Holder> sub holders(Queue log, int n) {
    return map new Holder(log, "h" + $1), xrange(1, n);
}

#! runs one case in a background thread, so that the log records every release before the case ends
string sub run(code<nothing(Queue)> c) {
    Queue log();
    background sub () {
        c(log);
        log.push("END");
    }();
    list<string> l;
    while (True) {
        string v = log.get(60s);
        if (v == "END") {
            break;
        }
        l += v;
    }
    return l.join("; ");
}

sub map_first(Queue log) {
    try {
        foreach Holder h in (map fail_on($1, "h1"), holders(log, 2)) {
            log.push("iter " + h.n);
        }
    } catch (hash<ExceptionInfo> ex) {
        log.push(sprintf("catch %s %s", ex.err, ex.desc));
    }
    log.push("after try");
}

sub map_later(Queue log) {
    try {
        foreach Holder h in (map fail_on($1, "h2"), holders(log, 3)) {
            log.push("iter " + h.n);
        }
    } catch (hash<ExceptionInfo> ex) {
        log.push(sprintf("catch %s %s", ex.err, ex.desc));
    }
    log.push("after try");
}

sub select_stage(Queue log) {
    try {
        foreach Holder h in (select holders(log, 3), select_fail_on($1, "h2")) {
            log.push("iter " + h.n);
        }
    } catch (hash<ExceptionInfo> ex) {
        log.push(sprintf("catch %s %s", ex.err, ex.desc));
    }
    log.push("after try");
}

sub take_drop(Queue log) {
    try {
        foreach Holder h in (map fail_on($1, "h3"), (take 3, (drop 1, holders(log, 4)))) {
            log.push("iter " + h.n);
        }
    } catch (hash<ExceptionInfo> ex) {
        log.push(sprintf("catch %s %s", ex.err, ex.desc));
    }
    log.push("after try");
}

sub captured_loop_variable(Queue log) {
    list<code> cl;
    try {
        foreach Holder h in (map fail_on($1, "h2"), holders(log, 3)) {
            cl += string sub () { return h.n; };
            log.push("iter " + h.n);
        }
    } catch (hash<ExceptionInfo> ex) {
        log.push(sprintf("catch %s %s", ex.err, ex.desc));
    }
    log.push(sprintf("closures %y", (map $1(), cl)));
    cl = ();
    log.push("after try");
}

sub body_throws(Queue log) {
    try {
        foreach Holder h in (map fail_on($1, "none"), holders(log, 3)) {
            log.push("iter " + h.n);
            if (h.n == "h2") {
                throw "BODY", h.n;
            }
        }
    } catch (hash<ExceptionInfo> ex) {
        log.push(sprintf("catch %s %s", ex.err, ex.desc));
    }
    log.push("after try");
}

sub nested_try(Queue log) {
    try {
        foreach Holder h in (map fail_on($1, "h3"), holders(log, 3)) {
            try {
                log.push("iter " + h.n);
                if (h.n == "h1") {
                    throw "INNER", h.n;
                }
            } catch (hash<ExceptionInfo> ex) {
                log.push(sprintf("inner catch %s %s", ex.err, ex.desc));
            }
        }
    } catch (hash<ExceptionInfo> ex) {
        log.push(sprintf("catch %s %s", ex.err, ex.desc));
    }
    log.push("after try");
}

sub main() {
    # every case runs more than once, so that the functions are compiled with tiered execution
    for (int i = 0; i < 3; ++i) {
        printf("map first: %s\n", run(\map_first()));
        printf("map later: %s\n", run(\map_later()));
        printf("select stage: %s\n", run(\select_stage()));
        printf("take drop: %s\n", run(\take_drop()));
        printf("captured loop variable: %s\n", run(\captured_loop_variable()));
        printf("body throws: %s\n", run(\body_throws()));
        printf("nested try: %s\n", run(\nested_try()));
    }
}

main();
