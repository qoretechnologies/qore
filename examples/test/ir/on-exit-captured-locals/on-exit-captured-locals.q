%modern
# on_exit, on_error and on_success handlers of blocks whose locals are captured by closures: the handlers see the
# current values of the locals, also those that a closure modified before and after the handler was declared, and the
# closures see the values that the handlers assigned; see on-exit-captured-locals.qtest, which runs this script in
# every execution mode and as an AOT executable, and checks that it gives the output of the AST interpreter (qore#5927)

#! a closure modifies captured locals before and after the on_exit handler is declared
string sub captured_before_after() {
    list<string> log;
    {
        string x = "a";
        string y = "b";
        bool flag;
        code cl = sub () { x += "1"; y += "2"; };
        cl();
        on_exit log += sprintf("exit x=%s y=%s flag=%y", x, y, flag);
        cl();
        flag = True;
    }
    return log.join("; ");
}

#! the handler assigns a captured local that the closure returned by the function reads after the function returned
code sub make_counter() {
    int n = 1;
    code get = int sub () { return n; };
    on_exit n *= 10;
    n += 2;
    return get;
}

#! the handlers of a block left by an exception see the values that a closure assigned
string sub exception_exit() {
    list<string> log;
    try {
        string s = "start";
        code cl = sub (string v) { s = v; };
        on_exit log += "exit s=" + s;
        on_error log += sprintf("error s=%s ex=%s", s, $1.err);
        on_success log += "success s=" + s;
        cl("before");
        throw "EX", "x";
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + ex.err;
    }
    return log.join("; ");
}

#! the handlers of a block left normally see the values that a closure assigned
string sub success_exit() {
    list<string> log;
    try {
        string s = "start";
        code cl = sub (string v) { s = v; };
        on_exit log += "exit s=" + s;
        on_error log += sprintf("error s=%s ex=%s", s, $1.err);
        on_success log += "success s=" + s;
        cl("after");
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + ex.err;
    }
    return log.join("; ");
}

#! handlers of nested blocks with captured locals of both blocks, left normally or by an exception
string sub nested_blocks(bool fail) {
    list<string> log;
    try {
        int a = 1;
        code inc_a = sub () { ++a; };
        on_exit log += sprintf("outer a=%d", a);
        for (int i = 0; i < 2; ++i) {
            int b = i * 10;
            code inc_b = sub () { b += 5; inc_a(); };
            on_exit log += sprintf("inner i=%d a=%d b=%d", i, a, b);
            inc_b();
            if (fail && i == 1) {
                throw "NB";
            }
        }
        inc_a();
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + ex.err;
    }
    return log.join("; ");
}

#! a closure with its own handler that sees its own captured local and one of the enclosing function
string sub closure_handler(bool fail) {
    list<string> log;
    int outer = 1;
    code c = sub (int v) {
        int own = v;
        code bump = sub () { ++own; ++outer; };
        on_exit log += sprintf("closure exit own=%d outer=%d", own, outer);
        bump();
        if (fail) {
            throw "CH";
        }
        bump();
    };
    try {
        c(10);
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + ex.err;
    }
    log += sprintf("after outer=%d", outer);
    return log.join("; ");
}

#! a return from a nested block runs the handlers of both blocks, which update a captured local through a closure
int sub early_return(reference<list<string>> log, bool early) {
    int r = 1;
    code dbl = sub () { r *= 2; };
    on_exit log += sprintf("exit r=%d", r);
    {
        on_exit dbl();
        if (early) {
            return r;
        }
        dbl();
    }
    return r;
}

#! closures created in a loop capture the local of their iteration, which the iteration's handler updates
string sub loop_closures() {
    list<code> cl;
    for (int i = 0; i < 3; ++i) {
        int v = i;
        cl += int sub () { return v; };
        on_exit v += 100;
    }
    return (map $1(), cl).join(",");
}

#! the method of the issue: a handler with a condition on a plain local, and locals captured by a closure
class C {
    private {
        int n = 0;
        list<string> log;
    }

    f(string a, string b, bool fail) {
        string x = a + b;
        string y = b + a;
        bool flag;
        on_exit if (!flag) {
            log += "not done x=" + x;
            ++n;
        }
        code cl = sub () {
            n += x.size() + y.size();
            x += "!";
            if (fail) {
                throw "CF";
            }
        };
        cl();
        flag = True;
    }

    string get() {
        return sprintf("n=%d log=%y", n, log);
    }
}

#! calls the method of the issue many times, normally and with exceptions
string sub method_calls() {
    C o();
    int caught = 0;
    for (int i = 0; i < 50; ++i) {
        try {
            o.f("a", "bc", i % 10 == 9);
        } catch (hash<ExceptionInfo> ex) {
            ++caught;
        }
    }
    return o.get() + sprintf(" caught=%d", caught);
}

#! an object whose member notification rejects a value
class Notified {
    public {
        int d = 1;
    }

    memberNotification(string member) {
        if (d == 13) {
            throw "NOTIFY-ERROR";
        }
    }
}

#! a member assignment raises an exception in a function with handlers; the catch block reads the object's local
#! (an on_error handler in the try block itself loses the local in the optimized modes: qore#5950)
string sub member_assignment_exception() {
    list<string> log;
    object g = new Notified();
    int n = 0;
    code inc = sub () { ++n; };
    on_exit log += sprintf("exit d=%d n=%d", g.d, n);
    try {
        g.d = 13;
    } catch (hash<ExceptionInfo> ex) {
        inc();
        log += sprintf("caught %s d=%d n=%d", ex.err, g.d, n);
    }
    return log.join("; ");
}

#! a member assignment raises an exception in a function without handlers; the catch block reads the object's local
string sub member_assignment_exception_no_handlers() {
    object g = new Notified();
    try {
        g.d = 13;
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("caught %s d=%d", ex.err, g.d);
    }
    return "not caught";
}

#! thread_exit leaves a block with handlers in a background thread; the handlers see the captured local
string sub thread_exit_handlers() {
    Queue q();
    background sub () {
        int v = 1;
        code inc = sub () { v += 10; };
        on_exit q.push(sprintf("exit v=%d", v));
        on_success q.push(sprintf("success v=%d", v));
        on_error q.push("error");
        inc();
        thread_exit;
    }();
    list<string> l = (q.get(), q.get());
    return l.join("; ");
}

for (int i = 0; i < 3; ++i) {
    printf("captured before and after: %s\n", captured_before_after());
    printf("handler assigns a captured local: %d\n", make_counter()());
    printf("exception exit: %s\n", exception_exit());
    printf("success exit: %s\n", success_exit());
    printf("nested blocks: %s\n", nested_blocks(False));
    printf("nested blocks with an exception: %s\n", nested_blocks(True));
    printf("closure handler: %s\n", closure_handler(False));
    printf("closure handler with an exception: %s\n", closure_handler(True));
    {
        list<string> log;
        int rv = early_return(\log, True);
        printf("early return: %d %s\n", rv, log.join("; "));
        log = ();
        rv = early_return(\log, False);
        printf("late return: %d %s\n", rv, log.join("; "));
    }
    printf("loop closures: %s\n", loop_closures());
    printf("method calls: %s\n", method_calls());
    printf("member assignment exception: %s\n", member_assignment_exception());
    printf("member assignment exception without handlers: %s\n", member_assignment_exception_no_handlers());
    printf("thread_exit: %s\n", thread_exit_handlers());
}
