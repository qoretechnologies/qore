%modern
%no-child-restrictions
%disable-warning unreferenced-variable
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the check at the start of every iteration of a loop statement sees every kind of request, and makes no lookup of the
# thread's sandbox manager while no interrupt is requested of any manager: a cancellation and a sandbox interrupt that
# another thread requests, the wall time limit of a sandbox call that the watchdog thread expires, and a memory limit
# that the loop exceeds itself

#! the kinds of loop statements
const Kinds = ("while", "do-while", "for", "foreach");

#! the iterations of a loop that must end
const Iterations = 1000;

#! True if the library counts the sandbox manager lookups of cancellation checks (debug builds)
const Counted = Option::HAVE_LIBRARY_DEBUGGING;

#! the code of the sandboxed Program: it calls the loops of this script, which its sandbox governs then
const SandboxCode = "%modern
auto sub call(code c, softlist<auto> args) {
    return call_function_args(c, args);
}
";

#! runs a loop statement of the given kind for \a n iterations, or until it is stopped if \a n is 0
/** @param started if given, decremented in the first iteration
*/
int sub spin(string kind, int n, *Counter started) {
    int i = 0;
    switch (kind) {
        case "while":
            while (!n || i < n) {
                if (++i == 1 && started) {
                    started.dec();
                }
            }
            break;
        case "do-while":
            do {
                if (++i == 1 && started) {
                    started.dec();
                }
            } while (!n || i < n);
            break;
        case "for":
            for (int j = 0; !n || j < n; ++j) {
                if (++i == 1 && started) {
                    started.dec();
                }
            }
            break;
        case "foreach":
            foreach int j in (xrange(0, n ? n : MAXINT)) {
                if (++i == 1 && started) {
                    started.dec();
                }
            }
            break;
    }
    return i;
}

#! runs a loop statement of the given kind that grows a string by 1 KB in each of its 10000 iterations
/** The memory limit of the sandbox stops it after about 1000 iterations
*/
int sub grow(string kind) {
    string s;
    string chunk = strmul("0123456789", 100);
    int n = 10000;
    int i = 0;
    switch (kind) {
        case "while":
            while (i < n) {
                s += chunk;
                ++i;
            }
            break;
        case "do-while":
            do {
                s += chunk;
                ++i;
            } while (i < n);
            break;
        case "for":
            for (int j = 0; j < n; ++j) {
                s += chunk;
                ++i;
            }
            break;
        case "foreach":
            foreach int j in (xrange(0, n)) {
                s += chunk;
                ++i;
            }
            break;
    }
    return i + s.size();
}

#! returns the result of a call that is stopped: the exception and the loop that raised it
string sub stopped(hash<ExceptionInfo> ex) {
    *list<*string> l = ex.desc =~ x/([a-z-]+ loop)/;
    return l ? sprintf("%s: %s", ex.err, l[0]) : ex.err;
}

#! returns a Program governed by the given sandbox manager
Program sub sandboxed(SandboxManager sm) {
    Program p(PO_NO_CHILD_PO_RESTRICTIONS);
    p.setSandboxManager(sm);
    p.parse(SandboxCode, "sandboxed");
    return p;
}

#! runs a loop in a background thread until it is stopped by \a stop, which is called once the loop has started
string sub run_stopped(code loop, code stop) {
    Queue res();
    Counter started(1);
    int tid = background sub () {
        try {
            loop(started);
            res.push("no error");
        } catch (hash<ExceptionInfo> ex) {
            # a cancellation stays pending; the rest of the thread must not be cancelled
            clear_thread_cancel();
            res.push(stopped(ex));
        }
    }();
    started.waitForZero();
    stop(tid);
    string r = res.get(60s);
    thread_join(tid);
    return r;
}

#! runs a loop that ends in the current thread and returns its result and whether its checks looked up the sandbox
#! manager: each of them must have with an interrupt requested and none without
string sub run_counted(code loop, bool lookups) {
    int start = Counted ? call_function("dbg_get_cancel_sandbox_lookup_count") : 0;
    int rv = loop();
    int count = Counted ? call_function("dbg_get_cancel_sandbox_lookup_count") - start : 0;
    bool ok = !Counted || (lookups ? count >= Iterations : !count);
    return sprintf("ok %d %s%s", rv, lookups ? "lookup" : "no-lookup", ok ? "" : sprintf(" (%d lookups)", count));
}

SandboxManager sm();
Program p = sandboxed(sm);

SandboxManager timed();
timed.setWallTimeLimit(100);
Program pt = sandboxed(timed);

SandboxManager limited();
limited.setMemoryLimit(1024 * 1024);
Program pm = sandboxed(limited);

foreach string kind in (Kinds) {
    # a cancellation requested by another thread
    printf("cancel-%s -> %s\n", kind, run_stopped(sub (Counter started) { spin(kind, 0, started); },
        sub (int tid) { cancel_thread(tid, "t"); }));

    # an interrupt of the sandbox requested by another thread
    printf("interrupt-%s -> %s\n", kind, run_stopped(
        sub (Counter started) { p.callFunction("call", \spin(), (kind, 0, started)); },
        sub (int tid) { sm.requestInterrupt(); }));
    sm.clearInterrupt();

    # a cleared interrupt does not stop the loop, and no check looks up the sandbox manager
    printf("cleared-%s -> %s\n", kind, run_counted(int sub () { return p.callFunction("call", \spin(), (kind, Iterations)); },
        False));

    # an interrupt of another sandbox makes the checks look up the manager, which has no interrupt requested
    SandboxManager other();
    other.requestInterrupt();
    printf("other-%s -> %s\n", kind, run_counted(int sub () { return p.callFunction("call", \spin(), (kind, Iterations)); },
        True));
    other.clearInterrupt();

    # no request: no lookup, also outside a sandbox
    printf("none-%s -> %s\n", kind, run_counted(int sub () { return spin(kind, Iterations); }, False));

    # a manager destroyed with its interrupt requested leaves none requested
    {
        SandboxManager gone();
        gone.requestInterrupt();
    }
    printf("destroyed-%s -> %s\n", kind, run_counted(int sub () { return spin(kind, Iterations); }, False));

    # the wall time limit of the call, expired by the watchdog thread
    try {
        pt.callFunction("call", \spin(), (kind, 0));
        printf("timeout-%s -> no error\n", kind);
    } catch (hash<ExceptionInfo> ex) {
        printf("timeout-%s -> %s\n", kind, stopped(ex));
    }

    # the memory limit of the sandbox, exceeded by the loop
    try {
        pm.callFunction("call", \grow(), kind);
        printf("memory-%s -> no error\n", kind);
    } catch (hash<ExceptionInfo> ex) {
        printf("memory-%s -> %s\n", kind, ex.err);
    }
}
