%modern
%disable-warning unreferenced-variable
# Copyright (C) 2026 Qore Technologies, s.r.o.
# a cancellation that another thread requests is seen at the start of the next iteration of a loop statement, and at
# no other point, whether the loop's polls are still unresolved (the first iteration of an invocation) or already read
# the thread's word inline (later iterations): see "Inline polls of native code" in design/cooperative-cancellation.md.
# Each case makes a cancellation pending at a known iteration (cancel_me(): another thread cancels this one while it
# waits with cancellation deferred), and reports the exception, its label, the iteration it was raised in, and the
# function and line of the place that raised it; every execution mode and AOT must report what the AST interpreter
# reports.  In debug builds, the number of full checks of a loop without a request is reported as well: one per
# iteration for the interpreters, one per invocation for native code, which the test compares by mode

#! the progress of a case: the iteration it reached
class Progress {
    public {
        int i = 0;
        int j = 0;
    }
}

#! makes a cancellation of the current thread pending: another thread cancels it while it waits with deferred
#! cancellation, so that the next cancellation point raises it
sub cancel_me() {
    Counter go(1);
    Counter cancelled(1);
    int tid = gettid();
    background sub () {
        go.waitForZero();
        cancel_thread(tid, "t");
        cancelled.dec();
    }();
    defer_thread_cancel(sub () {
        go.dec();
        cancelled.waitForZero();
    });
}

#! a while loop that makes the cancellation pending in iteration k (0: before the loop)
sub while_at(Progress p, int k) {
    if (!k) {
        cancel_me();
    }
    while (p.i < 10) {
        if (++p.i == k) {
            cancel_me();
        }
    }
}

#! a do-while loop that makes the cancellation pending in iteration k
sub do_while_at(Progress p, int k) {
    if (!k) {
        cancel_me();
    }
    do {
        if (++p.i == k) {
            cancel_me();
        }
    } while (p.i < 10);
}

#! a for loop that makes the cancellation pending in iteration k
sub for_at(Progress p, int k) {
    if (!k) {
        cancel_me();
    }
    for (int n = 0; n < 10; ++n) {
        if (++p.i == k) {
            cancel_me();
        }
    }
}

#! a foreach loop that makes the cancellation pending in iteration k
sub foreach_at(Progress p, int k) {
    if (!k) {
        cancel_me();
    }
    foreach int n in (xrange(10)) {
        if (++p.i == k) {
            cancel_me();
        }
    }
}

#! nested loops: the inner while loop makes the cancellation pending in its iteration 3 of the outer iteration 2; the
#! next check is the inner loop's
sub nested(Progress p) {
    foreach int n in (xrange(5)) {
        ++p.j;
        p.i = 0;
        while (p.i < 5) {
            if (++p.i == 3 && p.j == 2) {
                cancel_me();
            }
        }
    }
}

#! nested loops: the cancellation made pending in the outer loop after the inner loop ended: the next check is the
#! outer loop's
sub nested_outer(Progress p) {
    for (int n = 0; n < 5; ++n) {
        ++p.j;
        p.i = 0;
        while (p.i < 3) {
            ++p.i;
        }
        if (p.j == 2) {
            cancel_me();
        }
    }
}

#! the cancellation made pending in a try block in the loop: the check raises it in the scope of the loop, so the
#! loop's own catch block does not see it
sub in_try(Progress p) {
    while (p.i < 10) {
        try {
            if (++p.i == 4) {
                cancel_me();
            }
        } catch (hash<ExceptionInfo> ex) {
            throw "INNER-CATCH", ex.err;
        }
    }
}

#! a called function whose loop is entered with a cancellation pending: its first check raises it
sub callee_loop(Progress p) {
    int n = 0;
    while (n < 10) {
        ++n;
        ++p.j;
    }
}

#! the cancellation made pending in the loop of the caller before a call: the callee's first check raises it
sub call_at(Progress p) {
    while (p.i < 10) {
        if (++p.i == 3) {
            cancel_me();
            callee_loop(p);
        }
    }
}

#! a called function that makes the cancellation pending in its loop: its own next check raises it
sub callee_cancel(Progress p) {
    for (int n = 0; n < 10; ++n) {
        if (++p.j == 2) {
            cancel_me();
        }
    }
}

#! a loop that calls a function that makes the cancellation pending: the callee's loop raises it
sub call_in(Progress p) {
    while (p.i < 10) {
        ++p.i;
        callee_cancel(p);
    }
}

#! a loop in an on_exit block that makes the cancellation pending in its iteration 3
sub on_exit_loop(Progress p) {
    on_exit {
        while (p.i < 10) {
            if (++p.i == 3) {
                cancel_me();
            }
        }
    }
    ++p.j;
}

#! a pending cancellation in a deferral: the loop of the deferred code runs to its end, and the next check of the
#! caller's loop raises it
sub deferred(Progress p) {
    while (p.i < 3) {
        ++p.i;
        if (p.i == 2) {
            defer_thread_cancel(sub () {
                cancel_me();
                for (int n = 0; n < 50; ++n) {
                    ++p.j;
                }
            });
        }
    }
}

#! a cancellation cleared before the next check is not seen; a later one is
sub cleared(Progress p) {
    while (p.i < 10) {
        if (++p.i == 3) {
            cancel_me();
            clear_thread_cancel();
        } else if (p.i == 6) {
            cancel_me();
        }
    }
}

#! a class whose destructor makes the cancellation pending and runs a loop, which completes: the cancellation of a
#! trusted destructor is deferred until it returns
class Releaser {
    private {
        Progress p;
    }

    constructor(Progress p) {
        self.p = p;
    }

    destructor() {
        cancel_me();
        for (int n = 0; n < 20; ++n) {
            ++p.j;
        }
    }
}

#! a destructor that makes the cancellation pending in iteration 2: the loop of the destructor completes, and the
#! next check of the caller's loop raises it
sub dtor(Progress p) {
    while (p.i < 5) {
        if (++p.i == 2) {
            Releaser r(p);
            delete r;
        }
    }
}

#! the sandboxed Program whose request is out of scope for a thread that never entered it
our Program Sandboxed;
{
    SandboxManager sm();
    Sandboxed = new Program(PO_NEW_STYLE);
    Sandboxed.setSandboxManager(sm);
    Sandboxed.parse("%modern\nbool sub do_cancel(int tid) { return cancel_thread(tid, \"out of scope\"); }\n",
        "poll-points-sandboxed");
}

#! makes a request of the sandboxed Program pending for the current thread, which never entered it: the next check
#! drops it
sub cancel_me_out_of_scope() {
    Counter go(1);
    Counter cancelled(1);
    int tid = gettid();
    background sub () {
        go.waitForZero();
        Sandboxed.callFunction("do_cancel", tid);
        cancelled.dec();
    }();
    defer_thread_cancel(sub () {
        go.dec();
        cancelled.waitForZero();
    });
}

#! an out-of-scope request made pending in iteration 3 is dropped by the next check, and the loop continues with
#! inline polls; a request in scope in iteration 7 is seen by the next check
sub dropped(Progress p) {
    while (p.i < 10) {
        ++p.i;
        if (p.i == 3) {
            cancel_me_out_of_scope();
        } else if (p.i == 7) {
            cancel_me();
        }
    }
}

#! the cases: name -> code
const Cases = {
    "while-0": sub (Progress p) { while_at(p, 0); },
    "while-1": sub (Progress p) { while_at(p, 1); },
    "while-5": sub (Progress p) { while_at(p, 5); },
    "do-while-0": sub (Progress p) { do_while_at(p, 0); },
    "do-while-1": sub (Progress p) { do_while_at(p, 1); },
    "do-while-5": sub (Progress p) { do_while_at(p, 5); },
    "for-0": sub (Progress p) { for_at(p, 0); },
    "for-1": sub (Progress p) { for_at(p, 1); },
    "for-5": sub (Progress p) { for_at(p, 5); },
    "foreach-0": sub (Progress p) { foreach_at(p, 0); },
    "foreach-1": sub (Progress p) { foreach_at(p, 1); },
    "foreach-5": sub (Progress p) { foreach_at(p, 5); },
    "nested": \nested(),
    "nested-outer": \nested_outer(),
    "try": \in_try(),
    "call": \call_at(),
    "callee": \call_in(),
    "on-exit": \on_exit_loop(),
    "deferred": \deferred(),
    "cleared": \cleared(),
    "dtor": \dtor(),
    "dropped": \dropped(),
};

#! the order of the cases
const Order = ("while-0", "while-1", "while-5", "do-while-0", "do-while-1", "do-while-5", "for-0", "for-1", "for-5",
    "foreach-0", "foreach-1", "foreach-5", "nested", "nested-outer", "try", "call", "callee", "on-exit", "deferred",
    "cleared", "dtor", "dropped");

#! runs a case in a thread of its own and returns its result
string sub run(string name) {
    Queue res();
    code c = Cases{name};
    background sub () {
        Progress p();
        try {
            c(p);
            res.push(sprintf("ok i=%d j=%d", p.i, p.j));
        } catch (hash<ExceptionInfo> ex) {
            # the cancellation stays pending; the rest of the thread must not be cancelled
            clear_thread_cancel();
            *list<*string> l = ex.desc =~ x/^([a-z-]+ loop)/;
            # the place that raised it, and the user code frames of the stack
            list<string> stack = map sprintf("%s:%d", $1.function, $1.line), ex.callstack, $1.type == "user";
            res.push(sprintf("%s: %s i=%d j=%d at %d stack %y", ex.err, l ? l[0] : ex.desc, p.i, p.j, ex.line,
                stack));
        }
    }();
    return res.get(60s);
}

foreach string name in (Order) {
    printf("%s -> %s\n", name, run(name));
}

#! a loop of 1000 iterations without a request, in a function of its own
int sub spin(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += i;
    }
    return t;
}

# the number of full cancellation checks of three invocations of the loop, in debug builds (0 otherwise)
{
    spin(1);
    int start = Option::HAVE_LIBRARY_DEBUGGING ? call_function("dbg_get_cancel_check_count") : 0;
    spin(1000);
    spin(1000);
    spin(1000);
    int checks = Option::HAVE_LIBRARY_DEBUGGING ? call_function("dbg_get_cancel_check_count") - start : 0;
    stderr.printf("checks=%d\n", checks);
}
