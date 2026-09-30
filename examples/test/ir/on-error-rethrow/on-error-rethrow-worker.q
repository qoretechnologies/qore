#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# Worker for on-error-rethrow.qtest: runs each on_error / rethrow case and prints one
# "<case>: <outcome> [<handler log>]" line per case.  The qtest runs this script in every execution
# mode and as a qcc-compiled executable and compares every line with the expected outcome.

%modern

# the handlers record what they ran here; a global, so that every handler reaches it the same way in every mode
our list<string> log;

string sub chain(hash<ExceptionInfo> ex) {
    string rv = sprintf("%s:%s", ex.err, ex.desc);
    *hash<ExceptionInfo> n = ex.next;
    while (n) {
        rv += sprintf(" -> %s:%s", n.err, n.desc);
        n = n.next;
    }
    return rv;
}

sub run(string name, code c) {
    log = ();
    string res;
    try {
        auto rv = c();
        res = sprintf("returned %y", rv);
    } catch (hash<ExceptionInfo> ex) {
        res = "caught " + chain(ex);
    }
    printf("%s: %s [%s]\n", name, res, (foldl $1 + "," + $2, log) ?? "");
}

sub thrower(string err = "E", string desc = "d") {
    throw err, desc;
}

sub void_inline() {
    on_error push log, "oe";
    {
        on_error rethrow;
        throw "E", "d";
    }
    push log, "after-block";
}

int sub typed_call_args(bool x) {
    on_error if (x) {
        push log, "oe";
    }
    {
        on_error rethrow $1.err, "x";
        thrower();
    }
    return 1;
}

sub same_block() {
    on_error push log, "oe";
    on_error rethrow;
    throw "E", "d";
}

sub same_block_args() {
    on_error push log, "oe:" + $1.desc;
    on_error rethrow $1.err, $1.desc + "+a";
    thrower();
}

int sub outer_on_exit() {
    on_exit push log, "exit";
    {
        on_error rethrow $1.err, $1.desc + "+i";
        thrower();
    }
    return 2;
}

int sub outer_on_exit_if(bool x) {
    on_exit if (x) {
        push log, "exit-if";
    }
    {
        on_error rethrow;
        throw "E", "d";
    }
    return 3;
}

int sub outer_on_success() {
    on_success push log, "success";
    on_error push log, "oe";
    {
        on_error rethrow;
        thrower();
    }
    return 4;
}

int sub outer_on_success_if(bool x) {
    on_success if (x) {
        push log, "success-if";
    }
    {
        on_error rethrow $1.err, "s";
        thrower();
    }
    return 5;
}

int sub multi_level() {
    on_error push log, "oe0:" + $1.desc;
    {
        on_error rethrow $1.err, $1.desc + "+1";
        {
            on_error push log, "oe2:" + $1.desc;
            {
                on_error rethrow $1.err, $1.desc + "+3";
                {
                    on_error rethrow;
                    thrower();
                }
            }
        }
    }
    return 6;
}

int sub loop_call(list<auto> l) {
    on_error push log, "oe:" + $1.desc;
    int n = 0;
    foreach auto v in (l) {
        on_error rethrow $1.err, sprintf("%s (element %d)", $1.desc, $#);
        if (v) {
            thrower("L", "bad");
        }
        ++n;
    }
    return n;
}

int sub loop_inline(list<auto> l) {
    on_exit push log, "exit";
    on_error push log, "oe";
    int n = 0;
    foreach auto v in (l) {
        on_error rethrow;
        if (v) {
            throw "L", "inline";
        }
        ++n;
    }
    return n;
}

int sub outer_handler_throws() {
    on_error throw "H", "outer handler";
    {
        on_error rethrow $1.err, $1.desc + "+i";
        thrower();
    }
    return 7;
}

int sub inner_handler_throws() {
    on_error push log, "oe";
    {
        on_error throw "H", "inner handler";
        thrower();
    }
    return 8;
}

int sub both_rethrow() {
    on_error rethrow $1.err, $1.desc + "+outer";
    {
        on_error rethrow $1.err, $1.desc + "+inner";
        thrower();
    }
    return 9;
}

int sub no_exception() {
    on_error push log, "oe";
    on_success push log, "success";
    on_exit push log, "exit";
    {
        on_error rethrow $1.err, "never";
        push log, "body";
    }
    return 10;
}

int sub inner_no_rethrow() {
    on_error push log, "oe";
    {
        on_error push log, "inner-oe";
        thrower();
    }
    return 11;
}

int sub handler_catches_own() {
    on_error push log, "oe";
    {
        on_error {
            try {
                throw "X", "handled in handler";
            } catch (hash<ExceptionInfo> ex) {
                push log, "caught-in-handler:" + ex.err;
            }
        }
        thrower();
    }
    return 12;
}

int sub sequential_blocks() {
    on_error push log, "oe";
    {
        on_error rethrow;
        push log, "first";
    }
    {
        on_error rethrow $1.err, "second";
        thrower();
    }
    return 13;
}

int sub catch_in_body() {
    on_error push log, "oe";
    try {
        on_error rethrow $1.err, "in-try";
        thrower();
    } catch (hash<ExceptionInfo> ex) {
        push log, "catch:" + ex.desc;
    }
    return 14;
}

int sub rethrow_from_catch() {
    on_error push log, "oe:" + $1.desc;
    {
        on_error rethrow;
        try {
            thrower();
        } catch (hash<ExceptionInfo> ex) {
            rethrow ex.err, "from-catch";
        }
    }
    return 15;
}

string sub bad_arg() {
    throw "ARG", "rethrow argument failed";
}

int sub rethrow_args_throw() {
    on_error push log, "oe:" + $1.err;
    {
        on_error rethrow $1.err, bad_arg();
        thrower();
    }
    return 17;
}

class Poster {
    private scan(hash<auto> h) {
        foreach hash<auto> i in (h.pairIterator()) {
            on_error rethrow $1.err, sprintf("%s (key %y)", $1.desc, i.key);
            scan(i.value);
        }
    }

    private scan(auto v) {
        if (v.typeCode() == NT_OBJECT) {
            throw "EVENT-ERROR", "object";
        }
    }

    int post(hash<auto> h) {
        *hash<auto> lease = h.lease;
        bool transferred;
        bool retained;
        on_error if (h.key && !retained) {
            push log, "oe";
        }
        on_exit if (lease && !transferred) {
            push log, "release";
        }
        {
            on_error rethrow $1.err, $1.desc + " (event)";
            scan(h - "lease");
        }
        retained = True;
        transferred = True;
        return 16;
    }
}

run("void_inline", sub () { void_inline(); });
run("typed_call_args", auto sub () { return typed_call_args(True); });
run("typed_call_args_false", auto sub () { return typed_call_args(False); });
run("same_block", sub () { same_block(); });
run("same_block_args", sub () { same_block_args(); });
run("outer_on_exit", \outer_on_exit());
run("outer_on_exit_if", auto sub () { return outer_on_exit_if(True); });
run("outer_on_success", \outer_on_success());
run("outer_on_success_if", auto sub () { return outer_on_success_if(True); });
run("multi_level", \multi_level());
run("loop_call_ok", auto sub () { return loop_call((0, 0)); });
run("loop_call", auto sub () { return loop_call((0, 1, 0)); });
run("loop_inline", auto sub () { return loop_inline((0, 0, 1)); });
run("outer_handler_throws", \outer_handler_throws());
run("inner_handler_throws", \inner_handler_throws());
run("both_rethrow", \both_rethrow());
run("no_exception", \no_exception());
run("inner_no_rethrow", \inner_no_rethrow());
run("handler_catches_own", \handler_catches_own());
run("sequential_blocks", \sequential_blocks());
run("catch_in_body", \catch_in_body());
run("rethrow_from_catch", \rethrow_from_catch());
run("rethrow_args_throw", \rethrow_args_throw());
Poster p();
run("post_ok", auto sub () { return p.post({"key": "k", "info": 1, "lease": {}}); });
run("post_err", auto sub () { return p.post({"key": "k", "info": {"obj": new Mutex()}, "lease": {"a": 1}}); });
