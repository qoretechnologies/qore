#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The worker of on-exit-temporary-order.qtest: each case records when the temporaries of a statement are destroyed
# and when the on_exit, on_error and on_success handlers of the blocks that the statement leaves run, when an
# exception, a return or a break leaves them; the output must be the same in every execution mode as with the AST
# interpreter.  Each round calls every case again, so that the tiered and JIT modes run them as native code too.

%modern

#! the events of the current case
list<string> events;

#! an object that records its creation and its destruction
class Obj {
    private {
        string n;
    }

    constructor(string n) {
        self.n = n;
        events += "+" + n;
    }

    destructor() {
        events += "~" + n;
    }

    string name() {
        return n;
    }
}

#! an object whose destructor raises an exception
class ThrowingObj {
    destructor() {
        events += "~throwing";
        throw "DTOR-ERROR";
    }
}

sub take(auto a, auto b) {
}

auto sub raise() {
    throw "CASE-ERROR";
}

int sub one(auto a) {
    return 1;
}

sub event(string e) {
    events += e;
}

# an exception leaves the function
sub exit_handler() {
    on_exit event("on_exit");
    take(new Obj("t"), raise());
}

sub error_handler() {
    on_error event("on_error");
    on_success event("on_success");
    take(new Obj("t"), raise());
}

sub success_handler() {
    on_error event("on_error");
    on_success event("on_success");
    take(new Obj("t"), one(1));
}

sub two_handlers() {
    on_exit event("on_exit 1");
    on_exit event("on_exit 2");
    take(new Obj("t"), raise());
}

sub nested_blocks() {
    on_exit event("outer on_exit");
    {
        on_exit event("inner on_exit");
        take(new Obj("t"), raise());
    }
}

sub nested_block_local() {
    on_exit event("outer on_exit");
    {
        Obj l("local");
        on_exit event("inner on_exit");
        take(new Obj("t"), raise());
    }
}

sub function_local() {
    Obj l("local");
    on_exit event("on_exit");
    code c = sub () { l.name(); };
    take(new Obj("t"), raise());
}

int sub return_raises() {
    on_exit event("on_exit");
    return one(take(new Obj("t"), raise()));
}

sub loop_body_raises() {
    while (True) {
        on_exit event("on_exit");
        take(new Obj("t"), raise());
        break;
    }
}

sub method_chain() {
    on_exit event("on_exit");
    string s = new Obj("t").name() + raise();
}

sub throw_statement() {
    on_exit event("on_exit");
    throw "CASE-ERROR", new Obj("t").name();
}

sub if_block() {
    on_exit event("on_exit");
    if (one(1)) {
        take(new Obj("t"), raise());
    }
}

sub switch_case() {
    on_exit event("on_exit");
    switch (one(1)) {
        case 1:
            take(new Obj("t"), raise());
    }
}

sub catch_rethrows() {
    on_exit event("on_exit");
    try {
        take(new Obj("t"), raise());
    } catch () {
        event("catch");
        rethrow;
    }
}

sub try_handler() {
    try {
        on_exit event("on_exit");
        take(new Obj("t"), raise());
    } catch () {
        event("catch");
    }
}

sub destructor_raises() {
    on_exit event("on_exit");
    take(new ThrowingObj(), raise());
}

# the exception leaves the function of the case as well
sub caller_frame() {
    on_exit event("caller on_exit");
    take(new Obj("caller"), exit_handler());
}

# the list of a foreach statement is a temporary of a statement that encloses the blocks of its body
sub foreach_in_handler_block() {
    on_exit event("on_exit");
    foreach Obj o in ((new Obj("l1"), new Obj("l2"))) {
        take(new Obj("t"), raise());
    }
}

sub handler_in_foreach_body() {
    foreach Obj o in ((new Obj("l"),)) {
        on_exit event("on_exit");
        take(new Obj("t"), raise());
    }
}

sub handlers_in_and_around_foreach() {
    on_error event("outer on_error");
    foreach Obj o in ((new Obj("l"),)) {
        on_error event("inner on_error");
        on_success event("inner on_success");
        take(new Obj("t"), raise());
    }
}

sub division_in_foreach_body() {
    foreach Obj o in ((new Obj("l"),)) {
        on_exit event("on_exit");
        take(new Obj("t"), 1 / (o ? 0 : 1));
    }
}

sub throw_in_foreach_body() {
    foreach Obj o in ((new Obj("l"),)) {
        on_exit event("on_exit");
        throw "CASE-ERROR", new Obj("t").name();
    }
}

sub rethrow_in_foreach_body() {
    foreach Obj o in ((new Obj("l"),)) {
        on_exit event("on_exit");
        try {
            take(new Obj("t"), raise());
        } catch () {
            event("catch");
            rethrow;
        }
    }
}

sub foreach_in_loop_body() {
    while (True) {
        on_exit event("on_exit");
        foreach Obj o in ((new Obj("l"),)) {
            take(new Obj("t"), raise());
        }
        break;
    }
}

sub foreach_body_in_try() {
    try {
        foreach Obj o in ((new Obj("l"),)) {
            on_exit event("on_exit");
            take(new Obj("t"), raise());
        }
    } catch () {
        event("catch");
    }
}

sub foreach_in_try() {
    try {
        on_exit event("on_exit");
        foreach Obj o in ((new Obj("l"),)) {
            take(new Obj("t"), raise());
        }
    } catch () {
        event("catch");
    }
}

# a block with a local variable in the block with the handlers has the exception cleanup of its own
sub foreach_in_block_with_local() {
    on_exit event("on_exit");
    {
        Obj v("local");
        foreach Obj o in ((new Obj("l"),)) {
            take(new Obj("t"), raise());
        }
    }
}

sub block_with_local_in_foreach_body() {
    foreach Obj o in ((new Obj("l"),)) {
        on_exit event("on_exit");
        {
            Obj v("local");
            take(new Obj("t"), raise());
        }
    }
}

# a return or a break leaves the blocks
int sub return_value() {
    on_exit event("on_exit");
    return one(new Obj("t"));
}

sub break_in_condition() {
    while (True) {
        on_exit event("on_exit");
        if (one(new Obj("t"))) {
            break;
        }
    }
}

sub break_in_foreach() {
    foreach Obj o in ((new Obj("l"),)) {
        on_exit event("on_exit");
        take(new Obj("t"), one(1));
        break;
    }
    event("after");
}

int sub return_in_foreach() {
    foreach Obj o in ((new Obj("l"),)) {
        on_exit event("on_exit");
        return one(new Obj("t"));
    }
    return 0;
}

const Cases = (
    ("exit handler", \exit_handler()),
    ("error handler", \error_handler()),
    ("success handler", \success_handler()),
    ("two handlers", \two_handlers()),
    ("nested blocks", \nested_blocks()),
    ("nested block local", \nested_block_local()),
    ("function local", \function_local()),
    ("return raises", \return_raises()),
    ("loop body raises", \loop_body_raises()),
    ("method chain", \method_chain()),
    ("throw statement", \throw_statement()),
    ("if block", \if_block()),
    ("switch case", \switch_case()),
    ("catch rethrows", \catch_rethrows()),
    ("try handler", \try_handler()),
    ("destructor raises", \destructor_raises()),
    ("caller frame", \caller_frame()),
    ("foreach in handler block", \foreach_in_handler_block()),
    ("handler in foreach body", \handler_in_foreach_body()),
    ("handlers in and around foreach", \handlers_in_and_around_foreach()),
    ("division in foreach body", \division_in_foreach_body()),
    ("throw in foreach body", \throw_in_foreach_body()),
    ("rethrow in foreach body", \rethrow_in_foreach_body()),
    ("foreach in loop body", \foreach_in_loop_body()),
    ("foreach body in try", \foreach_body_in_try()),
    ("foreach in try", \foreach_in_try()),
    ("foreach in block with local", \foreach_in_block_with_local()),
    ("block with local in foreach body", \block_with_local_in_foreach_body()),
    ("return value", \return_value()),
    ("break in condition", \break_in_condition()),
    ("break in foreach", \break_in_foreach()),
    ("return in foreach", \return_in_foreach()),
);

for (int round = 0; round < 3; ++round) {
    printf("round %d\n", round);
    foreach list<auto> c in (Cases) {
        events = ();
        try {
            c[1]();
        } catch (hash<ExceptionInfo> ex) {
            string caught = "caught " + ex.err;
            *hash<ExceptionInfo> next = ex.next;
            while (next) {
                caught += " " + next.err;
                next = next.next;
            }
            events += caught;
        }
        printf("%s: %s\n", c[0], events.join(" "));
    }
}
