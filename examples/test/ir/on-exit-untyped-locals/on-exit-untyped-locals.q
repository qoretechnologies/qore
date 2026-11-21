%modern
# untyped locals of functions with on_exit, on_error and on_success handlers: a handler that creates no reference
# leaves them IR-only; see on-exit-untyped-locals.qtest, which runs this script in every execution mode and as an AOT
# executable, and checks that it gives the output of the AST interpreter (#6990)

#! the events that handlers record
our list<string> events;

#! untyped locals that the handler does not reference
string sub unrelated_handler(string a) {
    auto x = a + "x";
    auto y = x + "y";
    int n = 0;
    {
        on_exit ++n;
        x += "1";
    }
    return sprintf("%s %s %d", x, y, n);
}

#! as unrelated_handler(), with no parameter (a parameter is on the runtime stack)
string sub unrelated_handler_local() {
    auto x = "ax";
    auto y = x + "y";
    int n = 0;
    {
        on_exit ++n;
        x += "1";
    }
    return sprintf("%s %s %d", x, y, n);
}

#! untyped locals that the handlers read and assign, on a normal exit and on an exception caught in the function
string sub handler_assigns(bool fail) {
    auto s = "start";
    auto count = 0;
    try {
        on_exit {
            s += "/exit";
            ++count;
        }
        on_error s += "/error:" + $1.err;
        s = "body";
        if (fail) {
            throw "EX", "x";
        }
    } catch (hash<ExceptionInfo> ex) {
        s += "/caught";
    }
    return sprintf("%s %d", s, count);
}

#! adds to the value of a reference
sub add_ref(reference r, auto v) {
    r += v;
}

#! a handler that creates a reference to an untyped local
string sub handler_reference() {
    auto total = 1;
    {
        on_exit add_ref(\total, 10);
        total += 100;
    }
    return sprintf("%d", total);
}

#! an exception that leaves the function: the handlers read untyped locals
string sub exception_out() {
    events = ();
    try {
        throw_out();
    } catch (hash<ExceptionInfo> ex) {
        events += "caught " + ex.err;
    }
    return events.join(", ");
}

#! raises an exception after assigning the untyped local that its handlers read
sub throw_out() {
    auto state = "init";
    on_error events += "error state=" + state;
    on_exit events += "exit state=" + state;
    state = "assigned";
    throw "OUT", "x";
}

#! a handler with a functional operator that the AST evaluates
string sub handler_map() {
    auto l = (1, 2, 3);
    auto r;
    {
        on_exit r = (map $1 * 2, l, $1 > 1);
        l += 4;
    }
    return sprintf("%y", r);
}

#! untyped locals of a method with a handler that does not reference them
class C {
    private {
        int n = 0;
    }

    string f(string a) {
        auto x = a + "x";
        auto y = x + "y";
        on_exit ++n;
        return x + y;
    }

    int get() {
        return n;
    }
}

for (int i = 0; i < 3; ++i) {
    printf("unrelated handler: %s\n", unrelated_handler("a"));
    printf("unrelated handler, no parameter: %s\n", unrelated_handler_local());
    printf("handler assigns: %s\n", handler_assigns(False));
    printf("handler assigns with an exception: %s\n", handler_assigns(True));
    printf("handler reference: %s\n", handler_reference());
    printf("exception out of the function: %s\n", exception_out());
    printf("handler map: %s\n", handler_map());
    C c();
    string r = c.f("a") + c.f("b");
    printf("method: %s %d\n", r, c.get());
}
