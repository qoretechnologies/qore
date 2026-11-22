%modern
# on_exit, on_error and on_success handlers that read a local of their function that no closure captures and that
# holds NOTHING when an exception leaves the block: the handlers must run and read the local as NOTHING in every
# execution mode, as with a local that holds a value (#7244); see on-exit-nothing-locals.qtest, which runs this script
# in every execution mode and as an AOT executable, and checks that it gives the output of the AST interpreter

#! an object that a thread-local variable holds
class N {
    public {
        int x;
    }

    constructor(int x) {
        self.x = x;
    }
}

#! the thread-local variables that the handlers restore
thread_local *N tl;
thread_local *string ts;
thread_local *int ti;
thread_local *list<auto> tlist;
thread_local *hash<auto> thash;
thread_local int tn = 0;

#! the events that handlers record, in order
our list<string> events;

#! the value of the thread-local object variable, as text
string sub tlv() {
    return tl ? sprintf("N(%d)", tl.x) : "NOTHING";
}

#! raises an exception
sub thrower() {
    throw "BOOM", "boom";
}

#! on_exit: a nullable object local that holds NOTHING; the exception is caught in the function
string sub exit_object_caught(bool fail) {
    tl = NOTHING;
    try {
        *N previous = tl;
        on_exit tl = previous;
        tl = new N(1);
        if (fail) {
            thrower();
        }
    } catch (hash<ExceptionInfo> ex) {
        return "caught: " + tlv();
    }
    return "normal: " + tlv();
}

#! on_exit: the same with a value in the local
string sub exit_object_value_caught(bool fail) {
    tl = new N(7);
    try {
        *N previous = tl;
        on_exit tl = previous;
        tl = new N(1);
        if (fail) {
            thrower();
        }
    } catch (hash<ExceptionInfo> ex) {
        return "caught: " + tlv();
    }
    return "normal: " + tlv();
}

#! on_exit: the exception leaves the function, and the caller catches it
sub leaves_object() {
    *N previous = tl;
    on_exit tl = previous;
    tl = new N(2);
    thrower();
}

#! the caller of leaves_object()
string sub exit_object_leaves() {
    tl = NOTHING;
    try {
        leaves_object();
    } catch (hash<ExceptionInfo> ex) {
        return "caught: " + tlv();
    }
    return "no exception";
}

#! a nullable string local that holds NOTHING
string sub exit_string_caught() {
    ts = NOTHING;
    try {
        *string previous = ts;
        on_exit ts = previous;
        ts = "changed";
        thrower();
    } catch (hash<ExceptionInfo> ex) {
        return "caught: " + (ts ?? "NOTHING");
    }
    return "no exception";
}

#! a nullable int local that holds NOTHING, and an int local that holds 0
string sub exit_int_caught() {
    ti = NOTHING;
    tn = 0;
    try {
        *int previous = ti;
        int zero = tn;
        on_exit {
            ti = previous;
            tn = zero;
        }
        ti = 5;
        tn = 6;
        thrower();
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("caught: ti=%y tn=%d", ti, tn);
    }
    return "no exception";
}

#! nullable list and hash locals that hold NOTHING
string sub exit_container_caught() {
    tlist = NOTHING;
    thash = NOTHING;
    try {
        *list<auto> l = tlist;
        *hash<auto> h = thash;
        on_exit {
            tlist = l;
            thash = h;
        }
        tlist = (1, 2);
        thash = {"a": 1};
        thrower();
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("caught: tlist=%y thash=%y", tlist, thash);
    }
    return "no exception";
}

#! on_error: runs only for an exception, reads a local that holds NOTHING
string sub on_error_caught(bool fail) {
    tl = NOTHING;
    try {
        *N previous = tl;
        on_error tl = previous;
        on_success tl = new N(100);
        tl = new N(1);
        if (fail) {
            thrower();
        }
    } catch (hash<ExceptionInfo> ex) {
        return "caught: " + tlv();
    }
    return "normal: " + tlv();
}

#! on_success: runs only without an exception, reads a local that holds NOTHING
string sub on_success_caught(bool fail) {
    tl = NOTHING;
    try {
        *N previous = tl;
        on_success tl = previous;
        on_error tl = new N(200);
        tl = new N(1);
        if (fail) {
            thrower();
        }
    } catch (hash<ExceptionInfo> ex) {
        return "caught: " + tlv();
    }
    return "normal: " + tlv();
}

#! the handler tests the local with exists and records it
string sub exists_in_handler(bool fail) {
    events = ();
    try {
        *string nothing;
        *string value = "v";
        on_exit events += sprintf("exit nothing=%y value=%y", exists nothing, exists value);
        if (fail) {
            thrower();
        }
    } catch (hash<ExceptionInfo> ex) {
        events += "caught";
    }
    return events.join(", ");
}

#! one handler assigns the local that holds NOTHING and the handler that runs after it reads it
string sub assign_then_read(bool fail) {
    events = ();
    try {
        *string s;
        on_exit events += "second reads " + (s ?? "NOTHING");
        on_exit s = "assigned";
        on_exit events += "first reads " + (s ?? "NOTHING");
        if (fail) {
            thrower();
        }
    } catch (hash<ExceptionInfo> ex) {
        events += "caught";
    }
    return events.join(", ");
}

#! nested blocks: both blocks have handlers with locals that hold NOTHING, the inner block raises the exception
string sub nested_blocks(bool fail) {
    events = ();
    tl = NOTHING;
    ts = NOTHING;
    try {
        *N outer = tl;
        on_exit {
            events += "outer exit " + tlv();
            tl = outer;
        }
        tl = new N(1);
        {
            *string inner = ts;
            on_exit {
                events += "inner exit " + (ts ?? "NOTHING");
                ts = inner;
            }
            ts = "inner body";
            if (fail) {
                thrower();
            }
        }
        tl = new N(3);
    } catch (hash<ExceptionInfo> ex) {
        events += "caught " + tlv() + " " + (ts ?? "NOTHING");
    }
    events += "after " + tlv() + " " + (ts ?? "NOTHING");
    return events.join(", ");
}

#! a level of a recursion: the local holds NOTHING at the deepest level and a value at the others
sub recurse(int n) {
    *string state = n ? "level" : NOTHING;
    on_exit events += sprintf("exit n=%d state=%s", n, state ?? "NOTHING");
    on_error events += sprintf("error n=%d state=%s", n, state ?? "NOTHING");
    if (!n) {
        throw "REC", "bottom";
    }
    recurse(n - 1);
}

#! the recursion; the exception is caught by the caller
string sub recursion() {
    events = ();
    try {
        recurse(2);
    } catch (hash<ExceptionInfo> ex) {
        events += "caught " + ex.err;
    }
    return events.join(", ");
}

#! a loop: each iteration declares a local that holds NOTHING, and an iteration leaves by an exception, a break or a
#! continue
string sub loop_blocks() {
    events = ();
    for (int i = 0; i < 4; ++i) {
        try {
            *string s;
            on_exit events += sprintf("exit %d %s", i, s ?? "NOTHING");
            if (i == 1) {
                continue;
            }
            if (i == 2) {
                thrower();
            }
            if (i == 3) {
                break;
            }
            s = "set";
        } catch (hash<ExceptionInfo> ex) {
            events += "caught " + i;
        }
    }
    return events.join(", ");
}

#! a thread_exit leaves a block whose handler reads a local that holds NOTHING
string sub thread_exit_nothing() {
    Queue q();
    background thread_exit_body(q);
    return q.get();
}

#! the body of the thread of thread_exit_nothing()
sub thread_exit_body(Queue q) {
    *string s;
    on_exit q.push("exit " + (s ?? "NOTHING"));
    thread_exit;
}

#! a method whose handler restores a member that the method saved in a local
class C {
    private {
        *string state = "idle";
    }

    string run(bool fail) {
        *string previous;
        on_exit state = previous;
        state = "busy";
        if (fail) {
            thrower();
        }
        return "ok";
    }

    string get() {
        return state ?? "NOTHING";
    }
}

#! calls the method with and without an exception
string sub method_calls() {
    C c();
    string r;
    foreach bool fail in (False, True) {
        try {
            r += c.run(fail);
        } catch (hash<ExceptionInfo> ex) {
            r += "caught";
        }
        r += " state=" + c.get() + "; ";
    }
    return r;
}

for (int i = 0; i < 3; ++i) {
    foreach bool fail in (False, True) {
        printf("exit object (fail=%y): %s\n", fail, exit_object_caught(fail));
        printf("exit object with a value (fail=%y): %s\n", fail, exit_object_value_caught(fail));
        printf("on_error (fail=%y): %s\n", fail, on_error_caught(fail));
        printf("on_success (fail=%y): %s\n", fail, on_success_caught(fail));
        printf("exists in the handler (fail=%y): %s\n", fail, exists_in_handler(fail));
        printf("assign then read (fail=%y): %s\n", fail, assign_then_read(fail));
        printf("nested blocks (fail=%y): %s\n", fail, nested_blocks(fail));
    }
    printf("exit object, the exception leaves the function: %s\n", exit_object_leaves());
    printf("exit string: %s\n", exit_string_caught());
    printf("exit int: %s\n", exit_int_caught());
    printf("exit containers: %s\n", exit_container_caught());
    printf("recursion: %s\n", recursion());
    printf("loop: %s\n", loop_blocks());
    printf("thread_exit: %s\n", thread_exit_nothing());
    printf("method: %s\n", method_calls());
}
