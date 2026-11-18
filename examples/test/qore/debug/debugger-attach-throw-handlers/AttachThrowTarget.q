# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the functions that debugger-attach-throw-handlers.qtest steps through with AttachThrowDebugger.q; each caller calls
# attach_throw_callee() in a loop, which attaches the debugger in the iteration i == 2 and then raises an exception
# (when called with the debugger, the second time that it attaches it); the exception runs the handlers of the
# caller, whose step events the debugger gets; the target Program does not allow a debugger itself

%modern

our int attach_throw_calls = 0;

# attaches the debugger with a == True, and raises an exception when it does so the second time
string sub attach_throw_callee(bool a, code attach) {
    if (a) {
        attach();
    }
    string s = "abc";                                   ### callee_first
    if (a && ++attach_throw_calls > 1) {                ### callee_if
        throw "ATTACH-THROW";                           ### callee_throw
    }
    return s + "d";
}

# an on_exit handler of the function
auto sub caller_on_exit(int n, code attach, code detach, code log) {
    on_exit detach();                                   ### on_exit_function
    int total = 0;
    for (int i = 0; i < n; ++i) {
        total += attach_throw_callee(i == 2, attach).size();
    }
    return total;
}

# an on_error and an on_exit handler of the function
auto sub caller_on_error(int n, code attach, code detach, code log) {
    on_exit detach();                                   ### on_error_exit
    on_error log("error");                              ### on_error_error
    int total = 0;
    for (int i = 0; i < n; ++i) {
        total += attach_throw_callee(i == 2, attach).size();
    }
    return total;
}

# the handlers of a block in the loop and of the function
auto sub caller_block(int n, code attach, code detach, code log) {
    on_exit detach();                                   ### block_function
    int total = 0;
    for (int i = 0; i < n; ++i) {
        on_error log("block error " + i);               ### block_error
        on_exit log("block exit " + i);                 ### block_exit
        total += attach_throw_callee(i == 2, attach).size();
    }
    return total;
}
