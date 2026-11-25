%modern
# on_exit, on_error and on_success handlers that read and assign locals of their function that no closure captures:
# such a local is IR-only in the optimized execution modes, and it is put on the runtime local stack only while
# handlers run out of line (an exception, a thread_exit); see on-exit-ir-only-locals.qtest, which runs this script in
# every execution mode and as an AOT executable, and checks that it gives the output of the AST interpreter (#5951)

#! the events that destructors and handlers record, in order
our list<string> events;

#! an object whose destructor records its name
class D {
    public {
        string name;
    }

    constructor(string name) {
        self.name = name;
    }

    destructor() {
        events += "destroy " + name;
    }
}

#! the handlers of a block left normally assign a local that the code after the block reads
string sub normal_exit() {
    string s = "a";
    int n = 1;
    {
        on_exit s += sprintf("/exit n=%d", n);
        on_success s += "/success";
        on_error s += "/error";
        n = 2;
        s += "/body";
    }
    return s;
}

#! the handlers of a block left by an exception that the function catches assign a local that the catch block and
#! the code after the try statement read
string sub exception_same_function() {
    string s = "start";
    try {
        on_exit s += "/exit";
        on_error s += "/error:" + $1.err;
        on_success s += "/success";
        s = "body";
        throw "EX", "x";
    } catch (hash<ExceptionInfo> ex) {
        s += "/caught";
    }
    s += "/after";
    return s;
}

#! native numbers and a bool that the handlers of a block left by an exception assign
string sub typed_natives() {
    int i = 1;
    float f = 1.5;
    bool b = False;
    try {
        on_error {
            i += 10;
            f *= 2;
            b = True;
        }
        throw "EX", "x";
    } catch () {
    }
    return sprintf("i=%d f=%n b=%y", i, f, b);
}

#! a handler that assigns a local holding an object: the old object is destroyed by the assignment, in the handler
string sub destructor_order(bool fail) {
    events = ();
    {
        D d("first");
        try {
            on_exit {
                events += "handler start";
                d = new D("second");
                events += "handler end";
            }
            if (fail) {
                throw "EX", "x";
            }
        } catch () {
            events += "caught";
        }
        events += "block end " + d.name;
    }
    events += "after block";
    return events.join(", ");
}

#! the handlers of each frame of a recursion read the local of their own frame, when an exception leaves them all
string sub recursion() {
    events = ();
    try {
        recurse(3);
    } catch (hash<ExceptionInfo> ex) {
        events += "caught " + ex.err;
    }
    return events.join(", ");
}

#! a level of the recursion: the exception is raised by the deepest level
sub recurse(int n) {
    int depth = n * 10;
    on_error events += sprintf("error depth=%d", depth);
    on_exit events += sprintf("exit depth=%d", depth);
    if (!n) {
        throw "REC", "bottom";
    }
    depth += 1;
    recurse(n - 1);
}

#! an exception that leaves the function: the handlers read the local, and the caller catches the exception
string sub exception_out() {
    events = ();
    try {
        throw_out();
    } catch (hash<ExceptionInfo> ex) {
        events += "caught " + ex.err;
    }
    return events.join(", ");
}

#! raises an exception after assigning the local that its handlers read
sub throw_out() {
    string state = "init";
    on_error events += "error state=" + state;
    on_exit events += "exit state=" + state;
    state = "assigned";
    throw "OUT", "x";
}

#! a handler that captures a local in a closure and one that takes a reference to it: the local stays on the
#! runtime stack
string sub closure_and_reference() {
    int x = 1;
    int y = 1;
    {
        on_exit {
            code c = sub () { x += 10; };
            c();
        }
        on_exit inc(\y);
        x += 1;
        y += 1;
    }
    return sprintf("x=%d y=%d", x, y);
}

#! increments the value of a reference
sub inc(reference<int> r) {
    r += 100;
}

#! a thread_exit leaves a block whose handlers read and assign a local
string sub thread_exit_handlers() {
    Queue q();
    background thread_exit_body(q);
    return q.get();
}

#! the body of the thread of thread_exit_handlers()
sub thread_exit_body(Queue q) {
    string s = "start";
    on_exit q.push(s + "/exit");
    on_success s += "/success";
    on_error s += "/error";
    s = "body";
    thread_exit;
}

#! a method whose handler reads a local flag (the reproduction of the issue)
class C {
    private {
        int n = 0;
    }

    f(bool fail) {
        bool flag;
        on_exit if (!flag) {
            ++n;
        }
        if (fail) {
            throw "C", "x";
        }
        flag = True;
    }

    int get() {
        return n;
    }
}

#! calls the method normally and with an exception
string sub method_calls() {
    C c();
    int caught = 0;
    for (int i = 0; i < 6; ++i) {
        try {
            c.f((i % 2) == 1);
        } catch () {
            ++caught;
        }
    }
    return sprintf("n=%d caught=%d", c.get(), caught);
}

#! the object given to HandlerLocalParam::f(), which records when it is destroyed
class Arg {
    public {
        static int destroyed = 0;
    }

    destructor() {
        ++destroyed;
    }
}

#! a method whose block exit handlers run out of line on a normal exit, with a local that only a handler references
class HandlerLocalParam {
    f(Arg a) {
        {
            on_exit noop();
            int i = 1;
            on_error if (i > 10) {
                noop();
            }
        }
    }

    private noop() {
    }
}

#! the parameter of the method is released when the method returns: the AOT code left the variable that it put on
#! the runtime stack for the handlers there, and the parameter's value was never released
string sub handler_local_param() {
    int before = Arg::destroyed;
    HandlerLocalParam d();
    d.f(new Arg());
    return sprintf("destroyed=%d", Arg::destroyed - before);
}

for (int i = 0; i < 3; ++i) {
    printf("normal exit: %s\n", normal_exit());
    printf("exception in the same function: %s\n", exception_same_function());
    printf("typed natives: %s\n", typed_natives());
    printf("destructor order: %s\n", destructor_order(False));
    printf("destructor order with an exception: %s\n", destructor_order(True));
    printf("recursion: %s\n", recursion());
    printf("exception out of the function: %s\n", exception_out());
    printf("closure and reference: %s\n", closure_and_reference());
    printf("thread_exit: %s\n", thread_exit_handlers());
    printf("method calls: %s\n", method_calls());
    printf("parameter of a method with a handler local: %s\n", handler_local_param());
}
