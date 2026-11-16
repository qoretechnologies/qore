# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the functions that debugger-step-flow.qtest steps through with StepFlowDebugger.q; each one attaches the debugger
# while it runs, in the iteration i == 2 of its loop if it has one, and detaches it when it ends; the lines tagged
# "### <tag>" are the statements where the debugger requests a flow; the target Program does not allow a debugger itself

%modern

# a for loop whose condition is a fused compare-and-branch of two int locals
auto sub for_loop(int n, code attach, code detach, code log) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        if (i == 2) {                               ### for_body
            attach();
        }
        total += 10;                                ### for_first
        total += 1;                                 ### for_second
    }
    detach();
    return total;
}

# a while loop whose condition is a fused compare-and-branch of two int locals
auto sub while_loop(int n, code attach, code detach, code log) {
    int total = 0;
    int i = 0;
    while (i < n) {
        if (i == 2) {
            attach();
        }
        total += 10;                                ### while_first
        total += 1;
        ++i;
    }
    detach();
    return total;
}

# a while loop whose condition is a plain branch
auto sub while_call_loop(int n, code attach, code detach, code log) {
    int total = 0;
    int i = 0;
    while (log("cond" + i) && i < n) {
        if (i == 2) {
            attach();
        }
        total += 10;                                ### while_call_first
        total += 1;
        ++i;
    }
    detach();
    return total;
}

# a foreach loop over a list
auto sub foreach_loop(int n, code attach, code detach, code log) {
    int total = 0;
    foreach int i in (range(0, n - 1)) {
        if (i == 2) {
            attach();
        }
        total += 10;                                ### foreach_first
        total += 1;
    }
    detach();
    return total;
}

# nested loops: the flow is requested in the inner loop
auto sub nested_loops(int n, code attach, code detach, code log) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        if (i == 2) {
            attach();
        }
        for (int j = 0; j < 3; ++j) {
            total += 10;                            ### nested_inner
            total += 1;
        }
        total += 1000;                              ### nested_outer
    }
    detach();
    return total;
}

# a try statement in a loop
auto sub try_in_loop(int n, code attach, code detach, code log) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        if (i == 2) {
            attach();
        }
        try {
            total += 10;                            ### try_first
            total += 1;
        } catch (hash<ExceptionInfo> ex) {
            log("caught " + ex.err);
        }
    }
    detach();
    return total;
}

# a switch statement in a loop: a break in a case leaves the switch statement
auto sub switch_in_loop(int n, code attach, code detach, code log) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        if (i == 2) {
            attach();
        }
        switch (i) {
            case 3:
                total += 100;                       ### switch_case
                total += 1000;
                break;
            default:
                total += 1;
        }
        total += 10;
    }
    detach();
    return total;
}

# locals of the IR changed before the statement of the request in the same iteration
auto sub locals_changed(int n, code attach, code detach, code log) {
    float f = 0.0;
    int total = 0;
    for (int i = 0; i < n; ++i) {
        if (i == 2) {
            attach();
        }
        f = f + 1.5;
        int k = i * 2;
        total += k;                                 ### locals_after
    }
    detach();
    return f + total;
}

# handlers in the body of a loop
auto sub handlers_in_loop(int n, code attach, code detach, code log) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        if (i == 2) {
            attach();
        }
        on_exit log("exit" + i);
        on_success log("success" + i);
        on_error log("error" + i);
        total += 10;                                ### handlers_first
        total += 1;
    }
    detach();
    return total;
}

# no loop: a break leaves the function, which returns no value
auto sub no_loop(int n, code attach, code detach, code log) {
    int total = 0;
    attach();
    total += 1;
    on_exit log("exit");
    on_success log("success");
    total += 2;                                     ### no_loop_stmt
    detach();
    return total;
}

# a function that returns a value: a return at a step event returns none, which its return type does not accept
int sub int_return(int n, code attach, code detach, code log) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        if (i == 2) {
            attach();
        }
        total += 10;                                ### int_first
        total += 1;
    }
    detach();
    return total;
}

# an object that logs its destruction
class Tracker {
    private {
        code log;
        string name;
    }

    constructor(code log, string name) {
        self.log = log;
        self.name = name;
    }

    destructor() {
        log("destroy " + name);
    }
}

# a local of the loop's body that holds an object, which a flow out of the body destroys
auto sub object_local(int n, code attach, code detach, code log) {
    int total = 0;
    Tracker outer(log, "outer");
    for (int i = 0; i < n; ++i) {
        if (i == 2) {
            attach();
        }
        Tracker t(log, "t" + i);
        total += 10;                                ### object_first
        total += 1;
    }
    detach();
    return total;
}

# a foreach loop over a list of strings, whose iteration holds the list and its element
auto sub foreach_strings(int n, code attach, code detach, code log) {
    string all = "";
    int i = 0;
    foreach string s in (map sprintf("s%d", $1), range(0, n - 1)) {
        if (i++ == 2) {
            attach();
        }
        all += s;                                   ### foreach_strings_first
        all += ",";
    }
    detach();
    return all;
}

# a catch block in a loop
auto sub catch_in_loop(int n, code attach, code detach, code log) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        if (i == 2) {
            attach();
        }
        try {
            throw "ERR", sprintf("%d", i);
        } catch (hash<ExceptionInfo> ex) {
            total += 10;                            ### catch_first
            total += 1;
        }
    }
    detach();
    return total;
}

# called by caller(): the debugger is attached while both run
auto sub callee(int n, code attach, code log) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        if (i == 2) {
            attach();
        }
        total += 10;                                ### callee_first
        total += 1;
    }
    return total;
}

# a flow requested in a function that it calls: its own statements run on
auto sub caller(int n, code attach, code detach, code log) {
    auto r = callee(n, attach, log);
    log(sprintf("callee=%y", r));
    int x = 5;
    x += 1;
    detach();
    return x;
}

# a handler of the body of a loop with a request in it: the handler ends, and the block takes the request of its last
# handler run, as a break, a continue or a return of the block
auto sub handler_flow(int n, code attach, code detach, code log) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        if (i == 2) {
            attach();
        }
        on_exit {
            log("exit" + i);                        ### handler_flow_body
            log("exit2-" + i);
        }
        total += 10;
        total += 1;
    }
    detach();
    return total;
}

# two handlers: a request in the one that runs first is replaced by the outcome of the one that runs after it
auto sub handler_two(int n, code attach, code detach, code log) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        if (i == 2) {
            attach();
        }
        on_exit log("first" + i);
        on_exit {
            log("second" + i);                      ### handler_two_body
            log("second2-" + i);
        }
        total += 10;
        total += 1;
    }
    detach();
    return total;
}

# on_exit and on_error handlers of one block, which run out of line: a request in one ends it and is taken by the
# frame that runs it
auto sub handler_mixed(int n, code attach, code detach, code log) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        if (i == 2) {
            attach();
        }
        on_error log("error" + i);
        on_exit {
            log("mixed" + i);                       ### handler_mixed_body
            log("mixed2-" + i);
        }
        total += 10;
        total += 1;
    }
    detach();
    return total;
}

# a return requested at a statement, and another one in the handler that the first one runs
auto sub return_twice(int n, code attach, code detach, code log) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        if (i == 2) {
            attach();
        }
        on_exit {
            log("exit" + i);
            log("exit2-" + i);                      ### return_twice_handler
        }
        total += 10;                                ### return_twice_first
        total += 1;
    }
    detach();
    return total;
}

our int thread_exit_calls = 0;

# a thread_exit in a handler that a return requested at a step event runs
auto sub return_thread_exit(int n, code attach, code detach, code log) {
    int total = 0;
    ++thread_exit_calls;
    for (int i = 0; i < n; ++i) {
        if (i == 2) {
            attach();
        }
        on_exit {
            log("exit" + i);
            if (i == 3 && thread_exit_calls == 2) {
                thread_exit;
            }
        }
        total += 10;                                ### return_thread_exit_first
        total += 1;
    }
    detach();
    return total;
}

# exits the thread when destroyed
class ThreadExiter {
    public {
        code log;
    }
    constructor(code log) {
        self.log = log;
    }
    destructor() {
        log("destroy");
        thread_exit;
    }
}

our int dtor_exit_calls = 0;

# a thread_exit in the destructor of a local that a return requested at a step event releases
auto sub return_dtor_exit(int n, code attach, code detach, code log) {
    int total = 0;
    ++dtor_exit_calls;
    for (int i = 0; i < n; ++i) {
        if (i == 2) {
            attach();
        }
        *ThreadExiter t = dtor_exit_calls == 2 && i == 3 ? new ThreadExiter(log) : NOTHING;
        total += 10;                                ### return_dtor_exit_first
        total += 1;
    }
    detach();
    return total;
}

# a handler that raises an exception in a try block that a return requested at a step event leaves
auto sub return_throw_in_try(int n, code attach, code detach, code log) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        if (i == 2) {
            attach();
        }
        try {
            on_exit {
                if (i == 3) {
                    throw "HANDLER-ERR", "x";
                }
            }
            total += 10;                            ### return_throw_first
            total += 1;
        } catch (hash<ExceptionInfo> ex) {
            log("caught " + ex.err);
        }
    }
    detach();
    return total;
}

# a float local that the IR keeps in a native register (SSA promotion): a break request leaves the loop with the value
# that it has at the step event of the request
auto sub promoted_float(int n, code attach, code detach, code log) {
    attach();
    float f = 0.0;
    int total = 0;
    for (int i = 0; i < n; ++i) {
        f = f + 1.5;
        total += 1;                                 ### promoted_float_total
    }
    # no call after the loop: a call that the values of the locals are live across keeps them in memory
    return sprintf("f=%y total=%y", f, total);
}

# the return facts of a callee that the native code of its caller relies on (#6201): the code of each callee below
# never returns no value, while the path of a debugger's return request does, unless the declared return type of the
# callee requires a value; each caller calls the callee directly as native code, and detaches the debugger after the
# call that attaches it
# - a callee whose declared return type (auto) accepts no value, and whose code returns a string
auto sub facts_string(bool a, code attach) {
    if (a) {
        attach();
    }
    string s = "abc";                               ### facts_string_first
    return s + "d";
}

# - a callee whose declared return type requires a value: the return path raises the error of a missing return value
string sub facts_typed(bool a, code attach) {
    if (a) {
        attach();
    }
    string s = "abc";                               ### facts_typed_first
    return s + "d";
}

# - a callee with a native int return
int sub facts_int(bool a, code attach) {
    if (a) {
        attach();
    }
    int v = 40;                                     ### facts_int_first
    return v + 2;
}

# - a callee whose code returns a hash of a fixed shape
auto sub facts_hash(bool a, code attach) {
    if (a) {
        attach();
    }
    int v = 1;                                      ### facts_hash_first
    return {"x": v, "y": 2};
}

int sub facts_take_string(string s) {
    return s.size();
}

# the argument of a parameter that rejects no value: the check of the argument depends on the callee's return facts
auto sub facts_argument(int n, code attach, code detach, code log) {
    on_exit detach();
    int total = 0;
    for (int i = 0; i < n; ++i) {
        try {
            total += facts_take_string(facts_string(i == 2, attach));
        } catch (hash<ExceptionInfo> ex) {
            log(ex.err);
        }
        if (i == 2) {
            detach();
        }
    }
    return total;
}

# the value of a callee whose code returns a string
auto sub facts_value(int n, code attach, code detach, code log) {
    list<auto> l = ();
    for (int i = 0; i < n; ++i) {
        auto v = facts_string(i == 2, attach);
        push l, v.size();
        log(sprintf("%y", v));
        if (i == 2) {
            detach();
        }
    }
    return l;
}

# a callee whose declared return type requires a value
auto sub facts_typed_caller(int n, code attach, code detach, code log) {
    on_exit detach();
    int total = 0;
    for (int i = 0; i < n; ++i) {
        total += facts_typed(i == 2, attach).size();
    }
    return total;
}

# a native int return
auto sub facts_int_caller(int n, code attach, code detach, code log) {
    on_exit detach();
    int total = 0;
    for (int i = 0; i < n; ++i) {
        total += facts_int(i == 2, attach);
    }
    return total;
}

# a key of the value of a callee whose code returns a hash of a fixed shape
auto sub facts_projection(int n, code attach, code detach, code log) {
    list<auto> l = ();
    for (int i = 0; i < n; ++i) {
        push l, facts_hash(i == 2, attach).x;
        if (i == 2) {
            detach();
        }
    }
    return l;
}

# a debugger attached in a call that then raises an exception: the handlers that the exception runs in the caller get
# their step events, as with the AST interpreter (#6201)
our int attach_throw_calls = 0;
string sub attach_throw_callee(bool a, code attach) {
    if (a) {
        attach();
    }
    string s = "abc";                               ### attach_throw_first
    if (a && ++attach_throw_calls > 1) {
        throw "ATTACH-THROW";
    }
    return s + "d";
}

auto sub attach_throw_caller(int n, code attach, code detach, code log) {
    on_exit detach();
    int total = 0;
    for (int i = 0; i < n; ++i) {
        total += attach_throw_callee(i == 2, attach).size();
    }
    return total;
}

# the return facts of an immediately called closure, which the native code of a JIT caller relies on (#6201): the
# check of an argument of a callee's fast entry whose parameter rejects no value is left out when the closure never
# returns no value; the closure's path of a return request returns no value
int sub facts_take_int(int v) {
    return v + 1;
}

auto sub facts_closure_caller(int n, code attach, code detach, code log) {
    on_exit detach();
    int total = 0;
    for (int i = 0; i < n; ++i) {
        code c = auto sub () {
            if (i == 2) {
                attach();
            }
            int v = 40;                             ### facts_closure_first
            return v + 2;
        };
        try {
            total += facts_take_int(c());
        } catch (hash<ExceptionInfo> ex) {
            log(ex.err);
        }
    }
    return total;
}
