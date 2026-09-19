#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
#
# Worker for block-scope-unwind.qtest; that file holds the expected values.
#
# Each check prints one "<name>=<value>" line, so the caller can parse the
# output into a hash and assert per check regardless of ordering.
#
# The recurring value is the result of probing a Mutex from a scope-exit
# handler while an exception unwinds out of an inner lexical block holding an
# AutoLock on it:
#
#   RELEASED          the inner block's locals were uninstantiated before the
#                     handler ran -- the AST semantics every compiled tier has
#                     to reproduce
#   STILL-HELD:<err>  they were not, so the handler cannot take the Mutex
#
# Qore's Mutex is not recursive, so a second lock attempt from the owning
# thread fails immediately with LOCK-ERROR rather than blocking: nothing in
# this worker waits on anything.

%modern

our int fire_count;
our list<string> handler_order;

# reports whether m is free at the moment of the call
string sub probe(Mutex m) {
    try {
        AutoLock probe_lock(m);
        return "RELEASED";
    } catch (hash<ExceptionInfo> ex) {
        return "STILL-HELD:" + ex.err;
    }
}

# --- no exception: an inner block's AutoLock is released at block end --------

sub check_normal_block_exit() {
    Mutex m();
    {
        AutoLock al(m);
    }
    printf("normal-block-exit=%s\n", probe(m));
}

# --- on_error in an enclosing scope, AutoLock in a nested block --------------

sub on_error_nested(Mutex m) {
    on_error {
        printf("on_error-nested=%s\n", probe(m));
    }
    {
        AutoLock al(m);
        throw "BOOM";
    }
}

sub check_on_error_nested() {
    Mutex m();
    try {
        on_error_nested(m);
    } catch (hash<ExceptionInfo> ex) {
        # the handler's probe line is this check's result; the exception
        # itself is asserted by check_exception_identity()
    }
}

# --- the same shape with on_exit: this is not on_error-specific --------------

sub on_exit_nested(Mutex m) {
    on_exit {
        printf("on_exit-nested=%s\n", probe(m));
    }
    {
        AutoLock al(m);
        throw "BOOM";
    }
}

sub check_on_exit_nested() {
    Mutex m();
    try {
        on_exit_nested(m);
    } catch (hash<ExceptionInfo> ex) {
    }
}

# --- on_success, inner block exits normally ---------------------------------

sub on_success_normal(Mutex m) {
    on_success {
        printf("on_success-normal=%s\n", probe(m));
    }
    {
        AutoLock al(m);
    }
}

sub check_on_success_normal() {
    Mutex m();
    on_success_normal(m);
}

# --- doubly nested blocks ---------------------------------------------------

sub double_nested(Mutex m) {
    on_exit {
        printf("double-nested=%s\n", probe(m));
    }
    {
        {
            AutoLock al(m);
            throw "BOOM";
        }
    }
}

sub check_double_nested() {
    Mutex m();
    try {
        double_nested(m);
    } catch (hash<ExceptionInfo> ex) {
    }
}

# --- loop and conditional bodies are lexical scopes too ---------------------

sub foreach_body(Mutex m) {
    on_exit {
        printf("foreach-body=%s\n", probe(m));
    }
    foreach int i in (1..2) {
        AutoLock al(m);
        throw sprintf("BOOM-%d", i);
    }
}

sub check_foreach_body() {
    Mutex m();
    try {
        foreach_body(m);
    } catch (hash<ExceptionInfo> ex) {
    }
}

sub while_body(Mutex m) {
    on_exit {
        printf("while-body=%s\n", probe(m));
    }
    while (True) {
        AutoLock al(m);
        throw "BOOM";
    }
}

sub check_while_body() {
    Mutex m();
    try {
        while_body(m);
    } catch (hash<ExceptionInfo> ex) {
    }
}

# the condition is a parameter so that it cannot be folded away at parse time
sub if_body(Mutex m, bool cond) {
    on_exit {
        printf("if-body=%s\n", probe(m));
    }
    if (cond) {
        AutoLock al(m);
        throw "BOOM";
    }
}

sub check_if_body() {
    Mutex m();
    try {
        if_body(m, True);
    } catch (hash<ExceptionInfo> ex) {
    }
}

# --- try/catch in the same function: the LandingPad path, already correct ----

sub try_catch_same_fn(Mutex m) {
    try {
        {
            AutoLock al(m);
            throw "BOOM";
        }
    } catch (hash<ExceptionInfo> ex) {
        printf("try-catch-same-fn=%s\n", probe(m));
    }
}

sub check_try_catch_same_fn() {
    Mutex m();
    try_catch_same_fn(m);
}

# --- a block's own handler must still see its own locals alive --------------
# This is correct behaviour, not a bug: on_error here is in the same lexical
# scope as the AutoLock, so the lock is still held when the handler runs.  The
# fix for the nested shapes above must not "fix" this one.

sub own_scope_handler(Mutex m) {
    on_error {
        printf("own-scope-handler=%s\n", probe(m));
    }
    AutoLock al(m);
    throw "BOOM";
}

sub check_own_scope_handler() {
    Mutex m();
    try {
        own_scope_handler(m);
    } catch (hash<ExceptionInfo> ex) {
    }
}

# --- nested handler-bearing scopes ------------------------------------------
# The inner handler sees its own AutoLock live (as above); the outer handler,
# one scope further out, must see it released.

sub nested_handler_scopes(Mutex m) {
    on_exit {
        printf("nested-handlers-outer=%s\n", probe(m));
    }
    {
        {
            on_exit {
                printf("nested-handlers-inner=%s\n", probe(m));
            }
            AutoLock al(m);
            throw "BOOM";
        }
    }
}

sub check_nested_handler_scopes() {
    Mutex m();
    try {
        nested_handler_scopes(m);
    } catch (hash<ExceptionInfo> ex) {
    }
}

# --- handler firing order: innermost first ----------------------------------

sub ordered_handlers() {
    on_exit {
        push handler_order, "outer";
    }
    {
        on_exit {
            push handler_order, "inner";
        }
        throw "BOOM";
    }
}

sub check_handler_order() {
    handler_order = ();
    try {
        ordered_handlers();
    } catch (hash<ExceptionInfo> ex) {
    }
    printf("handler-order=%s\n", join(",", handler_order));
}

# --- each handler fires exactly once per scope exit -------------------------

sub fire_once_normal() {
    on_exit {
        ++fire_count;
    }
    {
        Mutex m();
        AutoLock al(m);
    }
}

sub check_fire_count_normal() {
    fire_count = 0;
    fire_once_normal();
    printf("fire-count-normal=%d\n", fire_count);
}

sub fire_once_error() {
    on_exit {
        ++fire_count;
    }
    {
        Mutex m();
        AutoLock al(m);
        throw "BOOM";
    }
}

sub check_fire_count_error() {
    fire_count = 0;
    try {
        fire_once_error();
    } catch (hash<ExceptionInfo> ex) {
    }
    printf("fire-count-error=%d\n", fire_count);
}

# --- the primary exception must reach the caller ----------------------------
# The handler takes the Mutex without catching: if the inner block's AutoLock
# were still held, the resulting LOCK-ERROR would displace OWNED-FAILURE.

sub identity_shape(Mutex m) {
    on_error {
        AutoLock recovery(m);
    }
    {
        AutoLock al(m);
        throw "OWNED-FAILURE", "the primary exception";
    }
}

sub check_exception_identity() {
    Mutex m();
    string err = "<none>";
    try {
        identity_shape(m);
    } catch (hash<ExceptionInfo> ex) {
        err = ex.err;
    }
    printf("exception-identity=%s\n", err);
}

# --- the same, for an exception caught and rethrown in the same function ----

sub rethrow_shape(Mutex m) {
    on_error {
        AutoLock recovery(m);
    }
    try {
        {
            AutoLock al(m);
            throw "OWNED-FAILURE", "the primary exception";
        }
    } catch (hash<ExceptionInfo> ex) {
        rethrow;
    }
}

sub check_rethrow_identity() {
    Mutex m();
    string err = "<none>";
    try {
        rethrow_shape(m);
    } catch (hash<ExceptionInfo> ex) {
        err = ex.err;
    }
    printf("rethrow-identity=%s\n", err);
}

# --- an exception raised by the handler itself ------------------------------

sub handler_throws_on_normal_exit() {
    on_exit {
        throw "HANDLER-FAILURE", "raised by the scope-exit handler";
    }
    {
        Mutex m();
        AutoLock al(m);
    }
}

sub check_handler_throw_normal() {
    string err = "<none>";
    try {
        handler_throws_on_normal_exit();
    } catch (hash<ExceptionInfo> ex) {
        err = ex.err;
    }
    printf("handler-throw-normal=%s\n", err);
}

# --- non-local exits out of a nested block ----------------------------------
# These take the compile-time cleanup_stack path rather than the unwind path;
# they are here so that the unwind fix cannot regress them unnoticed.

sub return_from_nested(Mutex m) {
    on_exit {
        printf("return-from-nested=%s\n", probe(m));
    }
    {
        AutoLock al(m);
        return;
    }
}

sub check_return_from_nested() {
    Mutex m();
    return_from_nested(m);
}

sub break_from_loop(Mutex m) {
    on_exit {
        printf("break-from-loop=%s\n", probe(m));
    }
    while (True) {
        AutoLock al(m);
        break;
    }
}

sub check_break_from_loop() {
    Mutex m();
    break_from_loop(m);
}

# each iteration must release its own AutoLock, or the second iteration's
# constructor raises LOCK-ERROR
sub continue_in_loop(Mutex m) {
    on_exit {
        printf("continue-in-loop=%s\n", probe(m));
    }
    int count = 0;
    while (count < 3) {
        ++count;
        AutoLock al(m);
        continue;
    }
}

sub check_continue_in_loop() {
    Mutex m();
    try {
        continue_in_loop(m);
    } catch (hash<ExceptionInfo> ex) {
    }
}

# --- the same shape reached through other lowering entry points -------------

class MethodContext {
    static go(Mutex m) {
        on_error {
            printf("method-context=%s\n", probe(m));
        }
        {
            AutoLock al(m);
            throw "BOOM";
        }
    }
}

sub check_method_context() {
    Mutex m();
    try {
        MethodContext::go(m);
    } catch (hash<ExceptionInfo> ex) {
    }
}

sub check_closure_context() {
    Mutex m();
    code c = sub () {
        on_error {
            printf("closure-context=%s\n", probe(m));
        }
        {
            AutoLock al(m);
            throw "BOOM";
        }
    };
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
    }
}

sub check_background_context() {
    Mutex m();
    Counter done(1);
    background sub () {
        on_exit {
            printf("background-context=%s\n", probe(m));
            done.dec();
        }
        try {
            {
                AutoLock al(m);
                throw "BOOM";
            }
        } catch (hash<ExceptionInfo> ex) {
        }
    }();
    done.waitForZero();
}

sub deep_nesting(Mutex m) {
    on_exit {
        printf("deep-nesting=%s\n", probe(m));
    }
    {
        {
            {
                {
                    AutoLock al(m);
                    throw "BOOM";
                }
            }
        }
    }
}

sub check_deep_nesting() {
    Mutex m();
    try {
        deep_nesting(m);
    } catch (hash<ExceptionInfo> ex) {
    }
}

# --- non-local exits and rethrow from inside a handler body -----------------
# These exercise the HandlerBarrier that keeps a handler from being re-inlined
# onto its own non-local exit path.

sub return_in_handler(Mutex m) {
    on_exit {
        printf("return-in-handler=%s\n", probe(m));
        return;
    }
    {
        AutoLock al(m);
        throw "BOOM";
    }
}

sub check_return_in_handler() {
    Mutex m();
    string outcome = "normal";
    try {
        return_in_handler(m);
    } catch (hash<ExceptionInfo> ex) {
        outcome = ex.err;
    }
    printf("return-in-handler-outcome=%s\n", outcome);
}

sub rethrow_in_handler(Mutex m) {
    on_error {
        printf("rethrow-in-handler=%s\n", probe(m));
        rethrow;
    }
    {
        AutoLock al(m);
        throw "PRIMARY", "the primary exception";
    }
}

sub check_rethrow_in_handler() {
    Mutex m();
    string err = "<none>";
    try {
        rethrow_in_handler(m);
    } catch (hash<ExceptionInfo> ex) {
        err = ex.err;
    }
    printf("rethrow-in-handler-err=%s\n", err);
    printf("rethrow-in-handler-after=%s\n", probe(m));
}

# an exception raised by the handler while another is already unwinding: the
# primary must still be the one the caller sees
sub handler_throws_on_error(Mutex m) {
    on_error {
        printf("handler-throw-error=%s\n", probe(m));
        throw "SECONDARY", "raised by the on_error handler";
    }
    {
        AutoLock al(m);
        throw "PRIMARY", "the primary exception";
    }
}

sub check_handler_throw_error() {
    Mutex m();
    string err = "<none>";
    try {
        handler_throws_on_error(m);
    } catch (hash<ExceptionInfo> ex) {
        err = ex.err;
    }
    printf("handler-throw-error-err=%s\n", err);
}

# --- the original Qorus shape: the handler runs under a cancel deferral ------
# Thread cancellation is not implicated in the unwind defect, but this is the
# shape the defect was first seen in, so it stays covered.  The deferral keeps
# the pending cancellation from turning the probe's own lock attempt into
# THREAD-CANCELLED.

sub defer_cancel_shape(Mutex m) {
    on_error {
        defer_thread_cancel(sub () {
            printf("defer-cancel=%s\n", probe(m));
        });
    }
    {
        AutoLock al(m);
        cancel_thread(gettid());
        throw "BOOM";
    }
}

sub check_defer_cancel() {
    Mutex m();
    try {
        defer_cancel_shape(m);
    } catch (hash<ExceptionInfo> ex) {
    }
    clear_thread_cancel();
    printf("defer-cancel-after=%s\n", probe(m));
}

# --- run everything ---------------------------------------------------------

check_normal_block_exit();
check_on_error_nested();
check_on_exit_nested();
check_on_success_normal();
check_double_nested();
check_foreach_body();
check_while_body();
check_if_body();
check_try_catch_same_fn();
check_own_scope_handler();
check_nested_handler_scopes();
check_handler_order();
check_fire_count_normal();
check_fire_count_error();
check_exception_identity();
check_rethrow_identity();
check_handler_throw_normal();
check_method_context();
check_closure_context();
check_background_context();
check_deep_nesting();
check_return_in_handler();
check_rethrow_in_handler();
check_handler_throw_error();
check_return_from_nested();
check_break_from_loop();
check_continue_in_loop();
check_defer_cancel();
